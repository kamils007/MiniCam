# Offline regression checks; runs on Windows PowerShell 5.1 and PowerShell 7.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/Build.ps1"
$sandbox = Join-Path ([IO.Path]::GetTempPath()) ('minicam-build-test-' + [guid]::NewGuid().ToString('N'))
$savedLocalAppData = $env:LOCALAPPDATA
$checks = 0

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
    $script:checks++
}
function Assert-Throws {
    param([scriptblock]$Action, [string]$Expected)
    $caught = $false
    try { & $Action }
    catch {
        if ($_.ToString() -notlike "*$Expected*") { throw }
        $caught = $true
    }
    Assert-True $caught "Expected failure: $Expected"
}

try {
    New-Item -ItemType Directory -Path $sandbox | Out-Null
    $env:LOCALAPPDATA = Join-Path $sandbox 'Local App Data'
    $statePath = Join-Path $sandbox 'out/station/environment.json'
    $defaultCache = Get-CacheLocation '' $statePath
    Assert-True ($defaultCache -eq (Join-Path $env:LOCALAPPDATA 'MiniCamTools/cache').Replace('\', '/')) 'Default cache must be local to the user.'
    $personalPresets = Join-Path $sandbox 'CMakeUserPresets.json'
    [IO.File]::WriteAllText($personalPresets, '{"version":3,"configurePresets":[]}')
    $personalHash = (Get-FileHash $personalPresets).Hash
    $unicode = 'cache ' + [char]0x0142 + [char]0x00f3 + [char]0x017a
    $selectedCache = Get-CacheLocation (Join-Path $sandbox $unicode) $statePath
    $state = [ordered]@{ schema = 1; cacheRoot = $selectedCache; lockHash = 'example' }
    Write-StationState $statePath $state
    $loaded = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-True ($loaded.cacheRoot -eq $selectedCache) 'JSON must preserve Unicode paths.'
    Assert-True ([IO.File]::ReadAllBytes($statePath)[0] -eq 123) 'State must not have a UTF-8 BOM.'
    # Set an old timestamp instead of waiting for filesystem timestamp resolution.
    $oldTime = [DateTime]::UtcNow.AddDays(-2)
    [IO.File]::SetLastWriteTimeUtc($statePath, $oldTime)
    $recordedTime = [IO.File]::GetLastWriteTimeUtc($statePath)
    Write-StationState $statePath $state
    Assert-True ([IO.File]::GetLastWriteTimeUtc($statePath) -eq $recordedTime) 'Repeated setup must not rewrite identical state.'
    Assert-True ((Get-CacheLocation '' $statePath) -eq $selectedCache) 'A shared cache selection must survive the next invocation.'
    $newCache = Join-Path $sandbox 'other cache'
    Assert-True ((Get-CacheLocation $newCache $statePath) -eq $newCache.Replace('\', '/')) 'Explicit cache must override remembered cache.'
    foreach ($invalid in @('bad,cache', 'bad;cache', 'bad#cache', 'bad%cache')) {
        Assert-Throws { Get-CacheLocation (Join-Path $sandbox $invalid) $statePath } 'Cache path cannot contain'
    }
    $state.cacheRoot = $newCache.Replace('\', '/')
    Write-StationState $statePath $state
    Assert-True ((Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json).cacheRoot -eq $state.cacheRoot) 'Changed setup must replace existing state.'
    Assert-True ((Get-FileHash $personalPresets).Hash -eq $personalHash) 'Do not overwrite personal presets.'
    Assert-True (@(Get-ChildItem (Split-Path $statePath) -Filter '*.tmp').Count -eq 0) 'State write must clean temporary files.'

    $git = (Get-Command git -ErrorAction Stop).Source
    $fakeVcpkg = Join-Path $sandbox 'managed vcpkg'
    New-Item -ItemType Directory -Path $fakeVcpkg | Out-Null
    Invoke-Checked $git @('-C', $fakeVcpkg, 'init', '--quiet')
    [IO.File]::WriteAllText((Join-Path $fakeVcpkg 'tracked.txt'), 'clean')
    Invoke-Checked $git @('-C', $fakeVcpkg, 'add', 'tracked.txt')
    Invoke-Checked $git @('-C', $fakeVcpkg, '-c', 'user.name=MiniCam test', '-c', 'user.email=test@example.invalid', 'commit', '--quiet', '-m', 'fixture')
    $fixtureCommit = & $git -C $fakeVcpkg rev-parse HEAD
    # The stub means bootstrap is unnecessary; no downloads occur in these checks.
    [IO.File]::WriteAllText((Join-Path $fakeVcpkg 'vcpkg.exe'), 'fixture only')
    Initialize-Vcpkg $git $fakeVcpkg ([pscustomobject]@{ vcpkgCommit = $fixtureCommit })
    $checks++
    Assert-Throws { Initialize-Vcpkg $git $fakeVcpkg ([pscustomobject]@{ vcpkgCommit = ('0' * 40) }) } 'Unexpected vcpkg checkout'
    [IO.File]::WriteAllText((Join-Path $fakeVcpkg 'tracked.txt'), 'user changes')
    Assert-Throws { Initialize-Vcpkg $git $fakeVcpkg ([pscustomobject]@{ vcpkgCommit = $fixtureCommit }) } 'modified tracked files'
    Assert-True ([IO.File]::ReadAllText((Join-Path $fakeVcpkg 'tracked.txt')) -eq 'user changes') 'Preparation must not reset a dirty checkout.'

    $engine = [Diagnostics.Process]::GetCurrentProcess().MainModule.FileName
    $failScript = Join-Path $sandbox 'failure.ps1'
    [IO.File]::WriteAllText($failScript, 'exit 7')
    Assert-Throws { Invoke-Checked $engine @('-NoProfile', '-File', $failScript) } 'Command failed (7)'
    Write-Host "Workstation script checks: $checks passed."
}
finally {
    $env:LOCALAPPDATA = $savedLocalAppData
    Remove-Item -LiteralPath $sandbox -Recurse -Force
}
