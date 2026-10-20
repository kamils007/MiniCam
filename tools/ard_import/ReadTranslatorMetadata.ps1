param([string]$Root, [string]$SolidLinkServer)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$report = [ordered]@{
    schema = 'minicam-ard-import-capabilities-v1'
    conversion_attempted = $false
    conversion_verified = $false
    vendor_objects_activated = $false
    license_configuration_modified = $false
    configuration_source = @{}
    files = @()
    error = $null
}
$out = Join-Path $PSScriptRoot ('wynik_' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($out)
$exitCode = 0
try {
    if ([IntPtr]::Size -ne 8 -or [Environment]::Version.Major -ne 4) {
        throw 'Wymagany Windows PowerShell 5.1 x64.'
    }
    # Match QSettings("MiniCAM", "MiniCAM") in AlphacamExport.cpp.
    # Read only the two non-secret path values, without enumerating the key.
    $settings = @{}
    foreach ($hive in @([Microsoft.Win32.RegistryHive]::CurrentUser, [Microsoft.Win32.RegistryHive]::LocalMachine)) {
        $base = $null
        $key = $null
        try {
            $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey($hive, [Microsoft.Win32.RegistryView]::Registry64)
            $key = $base.OpenSubKey('Software\MiniCAM\MiniCAM\alphacam', $false)
            if ($null -ne $key) {
                foreach ($name in @('root', 'solidlink')) {
                    if (-not $settings.ContainsKey($name)) {
                        $value = $key.GetValue($name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
                        if ($value -is [string] -and -not [string]::IsNullOrWhiteSpace($value)) {
                            $settings[$name] = $value
                            $report.configuration_source[$name] = $hive.ToString() + ':MiniCAM/alphacam/' + $name
                        }
                    }
                }
            }
        } finally {
            if ($null -ne $key) { $key.Dispose() }
            if ($null -ne $base) { $base.Dispose() }
        }
    }
    if ($Root) { $report.configuration_source['root'] = 'explicit_argument' }
    elseif ($settings.ContainsKey('root')) { $Root = $settings['root'] }
    else { throw 'Na tym koncie Windows nie znaleziono zapisanego ustawienia MiniCam alphacam/root. Uruchom test na tym samym koncie co dzialajacy zapis ARD.' }
    if ($SolidLinkServer) { $report.configuration_source['solidlink'] = 'explicit_argument' }
    elseif ($settings.ContainsKey('solidlink')) { $SolidLinkServer = $settings['solidlink'] }
    else { throw 'Na tym koncie Windows nie znaleziono zapisanego ustawienia MiniCam alphacam/solidlink. Uruchom test na tym samym koncie co dzialajacy zapis ARD.' }
    foreach ($required in @((Join-Path $Root 'Add-Ins\Feature\Feature.dll'),
                            (Join-Path $Root 'Add-Ins\SolidImport\SolidImport.dll'), $SolidLinkServer)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
            throw ('Zapisana konfiguracja MiniCam wskazuje nieistniejacy plik: ' + $required)
        }
    }
    Add-Type -Path (Join-Path $PSScriptRoot 'ReadTranslatorMetadata.cs')
    $files = @(
        (Join-Path $Root 'Add-Ins\Feature\Feature.dll'),
        (Join-Path $Root 'Add-Ins\SolidImport\SolidImport.dll'),
        $SolidLinkServer,
        (Join-Path (Split-Path -Parent $SolidLinkServer) 'SLInterop.dll')
    )
    foreach ($path in $files) {
        $entry = [ordered]@{ filename = [IO.Path]::GetFileName($path); exists = $false;
            version = $null; sha256 = $null; unchanged = $null; types = @();
            exports = @(); exports_error = $null; metadata_error = $null }
        try {
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Nie znaleziono: ' + $entry.filename) }
            $entry.exists = $true
            $entry.version = [Diagnostics.FileVersionInfo]::GetVersionInfo($path).FileVersion
            $entry.sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
            try { $entry.exports = @([MiniCamTranslatorMetadata]::ExportNames([IO.Path]::GetFullPath($path))) }
            catch { $entry.exports_error = $_.Exception.Message }
            try { $entry.types = @([MiniCamTranslatorMetadata]::Read([IO.Path]::GetFullPath($path))) }
            finally {
                $entry.unchanged = ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -eq $entry.sha256)
            }
            if (-not $entry.unchanged) { throw 'Plik zmienil sie podczas odczytu metadanych.' }
        } catch {
            $entry.metadata_error = $_.Exception.Message
            # Native DLLs may have no type library. Record that fact, not a
            # fabricated unsupported-format or license failure.
        }
        $report.files += $entry
    }
} catch { $report.error = $_.Exception.Message; $exitCode = 1 }
finally {
    $json = ConvertTo-Json -InputObject $report -Depth 16
    [IO.File]::WriteAllText((Join-Path $out 'translator_metadata.json'), $json, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host ('Raport: ' + (Join-Path $out 'translator_metadata.json'))
    Write-Host 'To odczyt metadanych. Konwersja bryly nie byla uruchamiana.'
}
exit $exitCode
