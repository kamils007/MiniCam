param([string]$Root, [string]$SolidLinkServer)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$report = [ordered]@{
    schema = 'minicam-ard-import-capabilities-v1'
    conversion_attempted = $false
    conversion_verified = $false
    vendor_objects_activated = $false
    license_configuration_modified = $false
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
    Add-Type -AssemblyName System.Windows.Forms
    if (-not $Root) {
        $pick = New-Object System.Windows.Forms.FolderBrowserDialog
        try {
            $pick.Description = 'Wskaz folder instalacji ALPHACAM uzywany przy zapisie ARD'
            if ($pick.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { throw 'Anulowano wybor instalacji.' }
            $Root = $pick.SelectedPath
        } finally { $pick.Dispose() }
    }
    if (-not $SolidLinkServer) {
        $default = Join-Path ${env:CommonProgramFiles} 'Hexagon\SolidLink 2025.4\code\bin\SolidLink.exe'
        if (Test-Path -LiteralPath $default -PathType Leaf) { $SolidLinkServer = $default }
        else {
            $pick = New-Object System.Windows.Forms.OpenFileDialog
            try {
                $pick.Title = 'Wskaz SolidLink.exe uzywany przy zapisie ARD'
                $pick.Filter = 'SolidLink.exe|SolidLink.exe'
                if ($pick.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { throw 'Anulowano wybor SolidLink.' }
                $SolidLinkServer = $pick.FileName
            } finally { $pick.Dispose() }
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
