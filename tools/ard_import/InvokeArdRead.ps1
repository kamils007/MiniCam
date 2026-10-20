param([string]$Ard)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
try {
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser,
        [Microsoft.Win32.RegistryView]::Registry64)
    $key = $null
    $settings = @{}
    try {
        $key = $base.OpenSubKey('Software\MiniCAM\MiniCAM\alphacam', $false)
        if ($null -eq $key) { throw 'Brak zapisanych ustawien eksportu MiniCam na tym koncie Windows.' }
        foreach ($name in @('root', 'runtime', 'python')) {
            $value = $key.GetValue($name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
            if (-not ($value -is [string]) -or [string]::IsNullOrWhiteSpace($value)) {
                throw ('Brak zapisanego ustawienia MiniCam: ' + $name)
            }
            $settings[$name] = $value
        }
    } finally {
        if ($null -ne $key) { $key.Dispose() }
        $base.Dispose()
    }
    if (-not $Ard) {
        Add-Type -AssemblyName System.Windows.Forms
        $pick = New-Object System.Windows.Forms.OpenFileDialog
        try {
            $pick.Title = 'Wybierz oryginalny testFAz.ard'
            $pick.Filter = 'Pliki ARD (*.ard)|*.ard'
            if ($pick.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { throw 'Anulowano wybor ARD.' }
            $Ard = $pick.FileName
        } finally { $pick.Dispose() }
    }
    if (-not (Test-Path -LiteralPath $settings['python'] -PathType Leaf)) { throw 'Nie znaleziono Pythona zapisanego w ustawieniach MiniCam.' }
    $out = Join-Path $PSScriptRoot ('odczyt_' + [Guid]::NewGuid().ToString('N'))
    Write-Host 'Zamknij ALPHACAM na czas testu, tak jak przy zapisie ARD.'
    & $settings['python'] -I -u (Join-Path $PSScriptRoot 'read_ard_body.py') --ard $Ard --runtime $settings['runtime'] --root $settings['root'] --out $out
    $result = $LASTEXITCODE
    if (Test-Path -LiteralPath (Join-Path $out 'raport_odczytu.zip') -PathType Leaf) {
        Write-Host ('Przeslij plik: ' + (Join-Path $out 'raport_odczytu.zip'))
    }
    exit $result
} catch {
    Write-Host ('Test nie wystartowal: ' + $_.Exception.Message)
    exit 1
}
