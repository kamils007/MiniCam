param(
    [Parameter(Mandatory=$true)][string]$ClassId,
    [Parameter(Mandatory=$true)][string]$InputModel,
    [Parameter(Mandatory=$true)][string]$OutputModel,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$translator = $null
$encoding = New-Object System.Text.UTF8Encoding($false)
$state = [ordered]@{
    schema = 'solidlink-conversion-worker-v1'
    class_id = $ClassId
    application_argument = 'Alphacam'
    activated = $false
    translation_attempted = $false
    license_configuration_modified = $false
    steps = @()
}

function Record-Step([string]$name, $value) {
    $entry = [ordered]@{step=$name; utc=[DateTime]::UtcNow.ToString('o'); value=$value}
    $state.steps += $entry
    $line = ConvertTo-Json -InputObject $entry -Depth 10 -Compress
    [IO.File]::AppendAllText((Join-Path $OutputDirectory 'worker_calls.jsonl'), $line + "`n", $encoding)
}

try {
    if ([IntPtr]::Size -ne 8) { throw 'Windows PowerShell x64 is required' }
    if ([IO.File]::Exists($OutputModel)) { throw 'Output already exists' }
    Record-Step 'before_com_activation' $null
    $comType = [Type]::GetTypeFromCLSID([Guid]$ClassId, $true)
    $translator = [Activator]::CreateInstance($comType)
    if ($null -eq $translator) { throw 'COM returned a null translator' }
    $state.activated = $true
    Record-Step 'after_com_activation' $true
    $state.input_file_type = [int]$translator.GetFileType($InputModel)
    Record-Step 'GetFileType' $state.input_file_type
    # Ordinary translation options only. Use the installed component's normal
    # authorization; no SendLicenseString/ConfirmParasolidLicensed calls.
    $null = $translator.SetOutputLogFile($true)
    Record-Step 'SetOutputLogFile' $true
    Record-Step 'before_TranslateSolid' $null
    $state.translation_attempted = $true
    $state.translate_return_code = [int]$translator.TranslateSolid($InputModel, $OutputModel, 'Alphacam')
    Record-Step 'TranslateSolid' $state.translate_return_code
    $state.interop_return_code = [int]$translator.GetInteropResult()
    Record-Step 'GetInteropResult' $state.interop_return_code
} catch {
    $errors = @()
    $exception = $_.Exception
    for ($index=0; $index -lt 8 -and $null -ne $exception; $index++) {
        $errors += [ordered]@{
            type=$exception.GetType().FullName
            message=$exception.Message
            hresult=('0x{0:X8}' -f ($exception.HResult -band 0xFFFFFFFFL))
        }
        $exception = $exception.InnerException
    }
    $state.error = $errors
    Record-Step 'error' $errors
} finally {
    if ($null -ne $translator) {
        try {
            $state.remaining_rcw_references = [Runtime.InteropServices.Marshal]::FinalReleaseComObject($translator)
            Record-Step 'released_com_object' $true
        } catch {
            $state.cleanup_error = $_.Exception.Message
        }
        $translator = $null
    }
    $json = ConvertTo-Json -InputObject $state -Depth 15
    $path = Join-Path $OutputDirectory 'worker_result.json'
    $stream = [IO.File]::Open($path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
    try {
        $bytes = $encoding.GetBytes($json)
        $stream.Write($bytes, 0, $bytes.Length)
    } finally {
        $stream.Dispose()
    }
}
if ($state.Contains('error')) { exit 1 }
exit 0
