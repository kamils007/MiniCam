$ErrorActionPreference = 'Stop'
try {
    if ([IntPtr]::Size -ne 8) { throw 'Uruchom test w Windows x64.' }
    Add-Type -AssemblyName System.Windows.Forms
    $bin = Join-Path $env:ProgramFiles 'Common Files\Hexagon\SolidLink 2025.4\code\bin'
    if (-not (Test-Path -LiteralPath (Join-Path $bin 'SPAIInterop.dll'))) {
        $folder = New-Object System.Windows.Forms.FolderBrowserDialog
        $folder.Description = 'Wskaz folder SolidLink 2025.4\code\bin'
        if ($folder.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { exit 1 }
        $bin = $folder.SelectedPath
        $folder.Dispose()
    }
    $candidates = New-Object System.Collections.Generic.List[string]
    $saved = Get-ItemProperty -LiteralPath 'HKCU:\Software\MiniCAM\MiniCAM\alphacam' -ErrorAction SilentlyContinue
    if ($saved -and $saved.python) { $candidates.Add([string]$saved.python) }
    foreach ($command in @(Get-Command python.exe,python3.exe,py.exe -ErrorAction SilentlyContinue)) {
        if ($command.Source -notmatch '\\WindowsApps\\') { $candidates.Add($command.Source) }
    }
    foreach ($root in @((Join-Path $env:LOCALAPPDATA 'Programs\Python'), (Join-Path $env:ProgramFiles 'Python'))) {
        if (Test-Path -LiteralPath $root) {
            foreach ($file in @(Get-ChildItem -LiteralPath $root -Filter python.exe -Recurse -ErrorAction SilentlyContinue)) {
                $candidates.Add($file.FullName)
            }
        }
    }
    $python = $null
    $code = "import struct,sys; print('MINICAM_PY64' if sys.version_info >= (3,8) and struct.calcsize('P')==8 else 'NO')"
    foreach ($candidate in $candidates) {
        if (-not (Test-Path -LiteralPath $candidate)) { continue }
        try {
            $check = & $candidate -I -c $code 2>$null
            if ($LASTEXITCODE -eq 0 -and $check -contains 'MINICAM_PY64') { $python = $candidate; break }
        } catch {}
    }
    if (-not $python) {
        $picker = New-Object System.Windows.Forms.OpenFileDialog
        $picker.Title = 'Wskaz python.exe x64 uzywany przez MiniCam'
        $picker.Filter = 'Python (python.exe)|python.exe'
        if ($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { exit 1 }
        $python = $picker.FileName
        $picker.Dispose()
    }
    $picker = New-Object System.Windows.Forms.OpenFileDialog
    $picker.Title = 'Wybierz plik ARD z jedna bryla do testu konwersji'
    $picker.Filter = 'ALPHACAM (*.ard)|*.ard'
    if ($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { exit 1 }
    $ard = $picker.FileName
    $picker.Dispose()
    $out = Join-Path $PSScriptRoot ('wynik_' + [Guid]::NewGuid().ToString('N'))
    Write-Host 'ALPHACAM nie musi byc uruchomiony. Test korzysta z zainstalowanych bibliotek.'
    Write-Host 'Po zakonczeniu przeslij raport_konwersji.zip z nowego folderu wynik_...'
    & $python -I -u (Join-Path $PSScriptRoot 'bulk_conversion_probe.py') --ard $ard --bin $bin --out $out
    if (Test-Path -LiteralPath $out) { Start-Process explorer.exe -ArgumentList ('"' + $out + '"') }
} catch {
    Write-Host ('Nie udalo sie uruchomic testu: ' + $_.Exception.Message)
    exit 1
}
