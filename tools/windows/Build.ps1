[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'DebugDLL')][string]$Configuration = 'Debug',
    [switch]$PrepareOnly,
    [switch]$ConfigureOnly,
    [string]$CacheRoot,
    [ValidateRange(1, 128)][int]$Jobs = 2
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

function Invoke-Checked {
    param([string]$File, [string[]]$Arguments)
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed ($LASTEXITCODE): $File $($Arguments -join ' ')" }
}

function Get-Git {
    $command = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    # Visual Studio may have inherited PATH before Git was installed.
    foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:LOCALAPPDATA)) {
        if (-not $base) { continue }
        foreach ($relative in @('Git/cmd/git.exe', 'Programs/Git/cmd/git.exe')) {
            $candidate = Join-Path $base $relative
            if (Test-Path -LiteralPath $candidate) { return $candidate }
        }
    }
    throw 'Install Git for Windows, then restart this script.'
}

function Find-VisualStudio {
    param($Lock)
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'Visual Studio Installer not found. Install VS 2022 17.14+ or Build Tools using .vsconfig.'
    }
    $savedEncoding = [Console]::OutputEncoding
    try {
        [Console]::OutputEncoding = New-Object Text.UTF8Encoding $false
        $json = & $vswhere -products '*' -version $Lock.visualStudioRange -requires `
            Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            Microsoft.VisualStudio.Component.VC.CMake.Project -format json -utf8
        if ($LASTEXITCODE -ne 0) { throw 'vswhere could not enumerate Visual Studio installations.' }
    }
    finally { [Console]::OutputEncoding = $savedEncoding }
    $installations = @((($json -join "`n") | ConvertFrom-Json) | Sort-Object installationVersion -Descending)
    foreach ($installation in $installations) {
        $path = $installation.installationPath
        $toolsets = @(Get-ChildItem -LiteralPath (Join-Path $path 'VC/Tools/MSVC') -Directory |
            Where-Object { $_.Name.StartsWith($Lock.msvcVersion + '.') } |
            Sort-Object Name -Descending)
        $cmake = Join-Path $path 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
        $ninja = Join-Path $path 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
        if ($toolsets.Count -gt 0 -and (Test-Path -LiteralPath $cmake) -and (Test-Path -LiteralPath $ninja)) {
            return [ordered]@{ path = $path; version = $installation.installationVersion
                msvc = $toolsets[0].Name; cmake = $cmake; ninja = $ninja }
        }
    }
    throw "VS 2022 with MSVC $($Lock.msvcVersion) x64 and CMake tools is required. Import .vsconfig in Visual Studio Installer."
}

function Import-DeveloperEnvironment {
    param($VisualStudio, $Lock)
    $env:MINICAM_VCVARS = Join-Path $VisualStudio.path 'VC/Auxiliary/Build/vcvarsall.bat'
    # Only fixed arguments from our lock are passed through cmd.exe.
    if ($env:MINICAM_VCVARS -match '["%!\r\n]' -or $Lock.msvcVersion -notmatch '^14\.\d+$' -or
        $Lock.windowsSdk -notmatch '^10\.0\.\d+\.0$') { throw 'Unsupported toolchain path or version.' }
    $command = 'call "%MINICAM_VCVARS%" amd64 ' + $Lock.windowsSdk +
        ' -vcvars_ver=' + $Lock.msvcVersion + ' >nul && set'
    # cmd /u emits Unicode for SET, preserving paths outside the console code page.
    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = $env:ComSpec
    $startInfo.Arguments = '/u /d /s /c ' + $command
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.StandardOutputEncoding = [Text.Encoding]::Unicode
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) { throw 'Could not start vcvarsall.' }
        $lines = $process.StandardOutput.ReadToEnd() -split "`r?`n"
        $process.WaitForExit()
        $result = $process.ExitCode
    }
    finally {
        $process.Dispose()
        Remove-Item Env:MINICAM_VCVARS -ErrorAction SilentlyContinue
    }
    if ($result -ne 0) {
        throw "Could not initialize MSVC / SDK $($Lock.windowsSdk). Import .vsconfig in Visual Studio Installer."
    }
    foreach ($line in $lines) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
        }
    }
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'vcvarsall did not provide cl.exe.' }
    if ($env:VCToolsVersion.TrimEnd('\') -notlike ($Lock.msvcVersion + '.*')) {
        throw 'vcvarsall selected an unexpected MSVC version.'
    }
    if ($env:WindowsSDKVersion.TrimEnd('\') -ne $Lock.windowsSdk) { throw 'Unexpected Windows SDK version.' }
}

function Assert-ToolVersion {
    param([string]$File, [string]$Minimum)
    $versionText = (& $File --version) -join "`n"
    if ($LASTEXITCODE -ne 0 -or $versionText -notmatch '(\d+\.\d+(?:\.\d+)?)') {
        throw "Cannot read tool version: $File"
    }
    if ([version]$matches[1] -lt [version]$Minimum) { throw "Update CMake tools in Visual Studio: $File needs $Minimum+." }
    return ($versionText -split "`n")[0]
}

function Get-CacheLocation {
    param([string]$Requested, [string]$StatePath)
    if ($Requested) { $selected = $Requested }
    elseif (Test-Path -LiteralPath $StatePath) {
        $selected = (Get-Content -LiteralPath $StatePath -Raw -Encoding UTF8 | ConvertFrom-Json).cacheRoot
    }
    else { $selected = Join-Path $env:LOCALAPPDATA 'MiniCamTools/cache' }
    $selected = [IO.Path]::GetFullPath($selected)
    # These are delimiters in vcpkg cache configuration or file URLs.
    if ($selected -match '[,;#%\r\n]') { throw 'Cache path cannot contain comma, semicolon, # or %.' }
    return $selected.Replace('\', '/')
}

function Initialize-Vcpkg {
    param([string]$Git, [string]$Root, $Lock)
    if ($Lock.vcpkgCommit -notmatch '^[0-9a-f]{40}$') { throw 'Invalid vcpkg commit in toolchain.lock.json.' }
    if (-not (Test-Path -LiteralPath $Root)) {
        $parent = Split-Path -Parent $Root
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
        $staging = Join-Path $parent ('preparing-' + [guid]::NewGuid().ToString('N'))
        try {
            Invoke-Checked $Git @('clone', '--no-checkout', 'https://github.com/microsoft/vcpkg.git', $staging)
            Invoke-Checked $Git @('-C', $staging, 'checkout', '--detach', $Lock.vcpkgCommit)
            Move-Item -LiteralPath $staging -Destination $Root
        }
        finally {
            if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
        }
    }
    $head = & $Git -C $Root rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $head -ne $Lock.vcpkgCommit) {
        throw "Unexpected vcpkg checkout: $Root. No files were reset. Use a clean managed checkout."
    }
    $dirty = @(& $Git -C $Root status --porcelain --untracked-files=no)
    if ($LASTEXITCODE -ne 0 -or $dirty.Count -gt 0) { throw "Managed vcpkg has modified tracked files: $Root" }
    $executable = Join-Path $Root 'vcpkg.exe'
    if (-not (Test-Path -LiteralPath $executable)) {
        Invoke-Checked (Join-Path $Root 'bootstrap-vcpkg.bat') @('-disableMetrics')
    }
    if (-not (Test-Path -LiteralPath $executable)) { throw 'vcpkg bootstrap did not produce vcpkg.exe.' }
}

function Write-StationState {
    param([string]$Path, $State)
    $json = ($State | ConvertTo-Json -Depth 10) + "`n"
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    # Preserve timestamps on repeated setup; do not rewrite personal CMake presets.
    if ((Test-Path -LiteralPath $Path) -and [IO.File]::ReadAllText($Path) -eq $json) { return }
    $temporary = $Path + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
    try {
        [IO.File]::WriteAllText($temporary, $json, (New-Object Text.UTF8Encoding $false))
        Move-Item -LiteralPath $temporary -Destination $Path -Force
    }
    finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force } }
}

function Invoke-MiniCamBuild {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { throw 'This script prepares Windows x64 workstations.' }
    if (-not [Environment]::Is64BitOperatingSystem) { throw 'Windows x64 is required.' }
    $lockPath = Join-Path $PSScriptRoot 'toolchain.lock.json'
    $lock = Get-Content -LiteralPath $lockPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $statePath = Join-Path $script:RepositoryRoot 'out/station/environment.json'
    $git = Get-Git
    $vs = Find-VisualStudio $lock
    Import-DeveloperEnvironment $vs $lock
    $cmakeVersion = Assert-ToolVersion $vs.cmake $lock.cmakeMinimum
    $ninjaVersion = Assert-ToolVersion $vs.ninja $lock.ninjaMinimum
    $cache = Get-CacheLocation $CacheRoot $statePath
    foreach ($directory in @('downloads', 'binaries', 'assets')) {
        New-Item -ItemType Directory -Force -Path (Join-Path $cache $directory) | Out-Null
    }
    # Keep source paths short enough for Windows build tools.
    $vcpkg = Join-Path $env:LOCALAPPDATA ('MiniCamTools/vcpkg/' + $lock.vcpkgCommit.Substring(0, 12))
    $env:VCPKG_ROOT = $vcpkg
    $env:VCPKG_DOWNLOADS = Join-Path $cache 'downloads'
    $env:VCPKG_DISABLE_METRICS = '1'
    $env:VCPKG_MAX_CONCURRENCY = "$Jobs"
    Initialize-Vcpkg $git $vcpkg $lock
    $state = [ordered]@{
        schema = 1; lockHash = (Get-FileHash -LiteralPath $lockPath -Algorithm SHA256).Hash.ToLowerInvariant()
        jobs = $Jobs
        vcpkgRoot = $vcpkg.Replace('\', '/'); cacheRoot = $cache
        visualStudio = $vs.path.Replace('\', '/'); visualStudioVersion = $vs.version
        msvc = $env:VCToolsVersion.TrimEnd('\'); sdk = $lock.windowsSdk
        cmake = $vs.cmake.Replace('\', '/'); cmakeVersion = $cmakeVersion
        ninja = $vs.ninja.Replace('\', '/'); ninjaVersion = $ninjaVersion
        git = $git.Replace('\', '/')
    }
    Write-StationState $statePath $state
    Write-Host "Ready: MSVC $($state.msvc), SDK $($state.sdk), $cmakeVersion, $ninjaVersion"
    Write-Host "vcpkg: $($lock.vcpkgCommit); cache: $cache"
    if ($PrepareOnly) {
        Write-Host 'Select station-debug / station-release in Visual Studio, or run KOMPILUJ.cmd.'
        return
    }
    $preset = @{ Debug = 'station-debug'; Release = 'station-release'; DebugDLL = 'station-debug-dll' }[$Configuration]
    Push-Location $script:RepositoryRoot
    try {
        Invoke-Checked $vs.cmake @('--preset', $preset)
        if ($ConfigureOnly) { return }
        Invoke-Checked $vs.cmake @('--build', '--preset', $preset, '--parallel', "$Jobs")
        Invoke-Checked (Join-Path (Split-Path -Parent $vs.cmake) 'ctest.exe') @('--preset', $preset)
        Write-Host "OK: out/build/$preset/minicam.exe"
    }
    finally { Pop-Location }
}

if ($MyInvocation.InvocationName -ne '.') {
    try { Invoke-MiniCamBuild }
    catch { Write-Error $_ -ErrorAction Continue; exit 1 }
}
