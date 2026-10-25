$ErrorActionPreference = 'Stop'
$utf8 = New-Object System.Text.UTF8Encoding($false)
$out = Join-Path $PSScriptRoot ('wynik_' + [Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $out
$report = [ordered]@{schema='minicam-translator-observation-v1'; conversion_verified=$false;
    observation='process polling; a missing observation is not proof that a converter was not used';
    processes=@(); configurations=@(); errors=@()}
$seen = @{}
$configSeen = @{}
$folders = New-Object 'System.Collections.Generic.HashSet[string]'
$configPaths = New-Object 'System.Collections.Generic.HashSet[string]'
$known = 'SpaImportConverterC.exe','ParasolidImportConverter.exe','ParasolidExportConverter.exe','SolidLink.exe'
Write-Host 'Przez 60 sekund rejestruje prace translatora.'
Write-Host 'TERAZ otworz plik STEP w ALPHACAM tak jak zwykle. Poczekaj na koniec testu.'
Write-Host 'Nie trzeba zamykac ALPHACAM po tescie.'
$until = [DateTime]::UtcNow.AddSeconds(60)
try {
    if ([IntPtr]::Size -ne 8) { throw 'Uruchom test w Windows PowerShell x64.' }
    do {
        try {
            $processes = @(Get-CimInstance Win32_Process -Filter "Name LIKE '%Converter%.exe' OR Name = 'SolidLink.exe'")
            foreach ($p in $processes) {
                $key = [string]$p.ProcessId + ':' + [string]$p.CreationDate
                # Store only executable metadata and filesystem arguments, never arbitrary command text.
                $paths = @()
                foreach ($m in [regex]::Matches([string]$p.CommandLine, '"([^"\r\n]+)"|([^\s"]+)')) {
                    $arg = if ($m.Groups[1].Success) { $m.Groups[1].Value } else { $m.Groups[2].Value }
                    $arg = $arg -replace '^/[pP]:',''
                    if ($arg -notmatch '^[A-Za-z]:\\') { continue }
                    if ([IO.Directory]::Exists($arg)) {
                        $null = $folders.Add($arg)
                        $paths += [ordered]@{kind='directory';path=$arg}
                    } elseif ([IO.File]::Exists($arg) -and [IO.Path]::GetExtension($arg) -eq '.cfg') {
                        $null = $folders.Add([IO.Path]::GetDirectoryName($arg))
                        $null = $configPaths.Add($arg)
                        $paths += [ordered]@{kind='configuration';path=$arg}
                    }
                }
                if (-not $seen.ContainsKey($key)) {
                    $seen[$key] = $true
                    $hash = $null
                    if ($p.ExecutablePath -and [IO.File]::Exists($p.ExecutablePath)) {
                        try { $hash = (Get-FileHash -LiteralPath $p.ExecutablePath -Algorithm SHA256).Hash.ToLowerInvariant() } catch {}
                    }
                    $report.processes += [ordered]@{utc=[DateTime]::UtcNow.ToString('o');pid=$p.ProcessId;
                        name=$p.Name;executable=$p.ExecutablePath;sha256=$hash;filesystem_arguments=$paths}
                }
            }
            $toRead = New-Object System.Collections.ArrayList
            foreach ($folder in $folders) { $null = $toRead.Add((Join-Path $folder 'ie_config.cfg')) }
            foreach ($cfgPath in $configPaths) { $null = $toRead.Add($cfgPath) }
            foreach ($cfg in $toRead) {
                if (-not [IO.File]::Exists($cfg)) { continue }
                try {
                    if ((Get-Item -LiteralPath $cfg).Length -gt 1048576) { continue }
                    $fields = [ordered]@{}
                    foreach ($line in Get-Content -LiteralPath $cfg) {
                        if ($line -match '^\s*(IE_SP_IE_DIR|IE_SP_FILE_TO_CONVERT|FFID|FILE_TYPE|INPUT_FORMAT|OUTPUT_FORMAT|TRANSLATION_METHOD|SINGLE_PROCESSOR)\s*=\s*(.*)$') {
                            $fields[$matches[1]] = $matches[2]
                        }
                    }
                    if ($fields.Contains('IE_SP_IE_DIR') -and [IO.Directory]::Exists($fields['IE_SP_IE_DIR'])) {
                        $null = $folders.Add($fields['IE_SP_IE_DIR'])
                    }
                    # Only allowlisted routing fields; no full CFG or authorization values are copied.
                    $encoded = ConvertTo-Json -InputObject $fields -Compress
                    if (-not $configSeen.ContainsKey($cfg) -or $configSeen[$cfg] -ne $encoded) {
                        $configSeen[$cfg] = $encoded
                        $report.configurations += [ordered]@{utc=[DateTime]::UtcNow.ToString('o');path=$cfg;fields=$fields}
                    }
                } catch { } # files may disappear when conversion finishes
            }
        } catch { $report.errors += $_.Exception.Message; break }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $until)
} catch { $report.errors += $_.Exception.Message }
finally {
    $report.observed_converter_count = @($report.processes | Where-Object {$_.name -in $known -and $_.name -ne 'SolidLink.exe'}).Count
    [IO.File]::WriteAllText((Join-Path $out 'translator_trace.json'), (ConvertTo-Json -InputObject $report -Depth 12), $utf8)
    $zip = Join-Path $out 'raport_translatora.zip'
    Compress-Archive -LiteralPath (Join-Path $out 'translator_trace.json') -DestinationPath $zip
    Write-Host ('Przeslij: ' + $zip)
    if ($report.observed_converter_count -eq 0) {
        Write-Host 'Nie zaobserwowano oczekiwanego konwertera. Raport nadal jest potrzebny; nie dowodzi to braku konwersji.'
    }
}
