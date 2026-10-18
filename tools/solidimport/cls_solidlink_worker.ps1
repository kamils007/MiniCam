param(
    [Parameter(Mandatory=$true)][string]$ClsRoot,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [Parameter(Mandatory=$true)][string]$SolidLinkServer,
    [Parameter(Mandatory=$true)][string]$ExpectedServerSha256,
    [Parameter(Mandatory=$true)][string]$ClassId,
    [Parameter(Mandatory=$true)][string]$InputModel,
    [Parameter(Mandatory=$true)][string]$OutputModel
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$state = [ordered]@{
    schema = 'cls-solidlink-worker-v1'
    phase = 'start'
    runtime_contract_checked = $false
    core_solids_verified = $false
    step_conversion_attempted = $false
    release_version_verified = $false
    cls_validated_before_translation = $false
    cls_validated_after_translation = $false
    conversion = [ordered]@{
        activated = $false
        translation_attempted = $false
        com_release_completed = $false
        application_argument = 'Alphacam'
    }
    close_attempted = $false
    close_completed = $false
}
$utf8 = New-Object System.Text.UTF8Encoding($false)
$resultPath = Join-Path $OutputDirectory 'worker_result.json'
$checkpointsPath = Join-Path $OutputDirectory 'worker_checkpoints.jsonl'
$openAttempted = $false
$closeMethod = $null
$succeeded = $false

function Save-Phase([string]$phase) {
    $state.phase = $phase
    $json = ConvertTo-Json -InputObject $state -Depth 8
    [IO.File]::WriteAllText($resultPath, $json, $utf8)
    $entry = [ordered]@{ phase = $phase; utc = [DateTime]::UtcNow.ToString('o') }
    [IO.File]::AppendAllText($checkpointsPath, (ConvertTo-Json -Compress -InputObject $entry) + "`n", $utf8)
}

function Assert-NoAcam {
    if (@(Get-Process -Name 'Acam' -ErrorAction SilentlyContinue).Count -gt 0) {
        throw 'Acam.exe is running; this test requires the application to remain closed.'
    }
}

function Get-PublicMethod([string]$name, [string[]]$parameters, [string]$returns) {
    $found = @($script:licenseType.GetMethods([Reflection.BindingFlags]'Public,Static') |
        Where-Object {
            if ($_.Name -ne $name -or $_.ReturnType.FullName -ne $returns) { return $false }
            $actual = @($_.GetParameters() | ForEach-Object { $_.ParameterType.FullName })
            if ($actual.Count -ne $parameters.Count) { return $false }
            for ($i = 0; $i -lt $actual.Count; $i++) {
                if ($actual[$i] -ne $parameters[$i]) { return $false }
            }
            return $true
        })
    if ($found.Count -ne 1) { throw "Unexpected public API contract: $name" }
    return $found[0]
}

function Get-CheckedEnum([Type]$type, [string]$name, [int]$number) {
    if (-not $type.IsEnum) { throw "Not an enum: $($type.FullName)" }
    $value = [Enum]::Parse($type, $name, $false)
    if ([Convert]::ToInt32($value) -ne $number) { throw "Unexpected enum value: $name" }
    return $value
}

function Get-PublicLicenseEnum([string]$name) {
    # These enums are public nested types, not assembly-level types.
    $type = $script:licenseType.GetNestedType($name, [Reflection.BindingFlags]::Public)
    if ($null -eq $type -or -not $type.IsNestedPublic -or -not $type.IsEnum -or
        $type.DeclaringType -ne $script:licenseType) {
        throw "Unexpected nested public enum: PlanitCls.License+$name"
    }
    return $type
}

try {
    Save-Phase 'preflight'
    if ([IntPtr]::Size -ne 8 -or [Environment]::Version.Major -ne 4) {
        throw 'Use Windows PowerShell x64 with .NET Framework v4.'
    }
    Assert-NoAcam
    $selectedClass = [Guid]$ClassId
    if ($selectedClass -eq [Guid]::Empty) { throw 'Empty SolidLink class.' }
    if (@(Get-Process -Name 'SolidLink' -ErrorAction SilentlyContinue).Count -gt 0) {
        throw 'An existing SolidLink process was found; no existing translator session will be reused.'
    }
    $serverHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $SolidLinkServer).Hash.ToLowerInvariant()
    if ($ExpectedServerSha256 -notmatch '^[0-9a-f]{64}$' -or $serverHash -ne $ExpectedServerSha256) {
        throw 'SolidLink changed after the installation preflight.'
    }
    $version = [Diagnostics.FileVersionInfo]::GetVersionInfo($SolidLinkServer)
    $state.solidlink_file_major = $version.FileMajorPart
    $state.solidlink_file_minor = $version.FileMinorPart
    $state.solidlink_file_version = $version.FileVersion
    if ($version.FileMajorPart -ne 2025 -or $version.FileMinorPart -ne 4) {
        throw 'The selected SolidLink executable is not version 2025.4.'
    }
    $classRoot = 'HKEY_CLASSES_ROOT\CLSID\' + $selectedClass.ToString('B')
    $serverCommand = [Microsoft.Win32.Registry]::GetValue($classRoot + '\LocalServer32', '', $null)
    $inproc = [Microsoft.Win32.Registry]::GetValue($classRoot + '\InprocServer32', '', $null)
    if ($null -ne $inproc) { throw 'Unexpected in-process COM server.' }
    $serverCommand = [Environment]::ExpandEnvironmentVariables([string]$serverCommand).Trim()
    if ($serverCommand -match '^"([^"]+)"(?:\s.*)?$') { $registeredPath = $Matches[1] }
    elseif ($serverCommand -match '^(.+?\.exe)(?:\s.*)?$') { $registeredPath = $Matches[1] }
    else { throw 'Cannot read the selected COM server path.' }
    if ([IO.Path]::GetFullPath($registeredPath) -ine [IO.Path]::GetFullPath($SolidLinkServer)) {
        throw 'COM registration changed after preflight.'
    }
    Save-Phase 'before_solidlink_api_contract'
    Add-Type -Path (Join-Path $PSScriptRoot 'solidlink_public_contract.cs') -ReferencedAssemblies 'System.dll','System.Core.dll'
    $state.solidlink_api_contract = [MiniCamSolidLinkMetadata]::Verify($SolidLinkServer)
    $state.solidlink_api_contract_verified = $true
    Save-Phase 'after_solidlink_api_contract'
    if (-not [IO.File]::Exists($InputModel) -or [IO.File]::Exists($OutputModel)) {
        throw 'Input is missing or output already exists.'
    }
    $ClsRoot = [IO.Path]::GetFullPath($ClsRoot)
    $expected = @{
        'LicManaged.dll' = '0b30282755e72feefdc3b43e50c0628749e49a5bd97fa06dabc002a0851a4827'
        'CLS.dll' = '5d1e001654690774794b2d6093491e6b0b51aa670b6694f6c98883c29b8f02c8'
        'ClsInit.exe' = 'd594c321383c33ca4eb12eedf3f95cb2adfc269484cdea986fbe5973737ef252'
    }
    foreach ($name in $expected.Keys) {
        $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $ClsRoot $name)).Hash
        if ($actual.ToLowerInvariant() -ne $expected[$name]) { throw "Unexpected module build: $name" }
    }
    # Dependency search changes are local to this disposable PowerShell process.
    $env:PATH = $ClsRoot + ';' + (Split-Path -Parent $ClsRoot) + ';' + $env:PATH
    Save-Phase 'before_assembly_load'
    $assembly = [Reflection.Assembly]::LoadFrom((Join-Path $ClsRoot 'LicManaged.dll'))
    $script:licenseType = $assembly.GetType('PlanitCls.License', $true)
    if (-not $licenseType.IsPublic) { throw 'PlanitCls.License is not public.' }
    Save-Phase 'before_api_contract'
    $productType = Get-PublicLicenseEnum 'Product'
    $showType = Get-PublicLicenseEnum 'ShowError'
    $openResultType = Get-PublicLicenseEnum 'OpenLicenseConnectionResult'
    $validResultType = Get-PublicLicenseEnum 'ValidLicenseConnectionResult'
    $state.api_types = @{
        product = $productType.FullName
        show_error = $showType.FullName
        open_result = $openResultType.FullName
        validation_result = $validResultType.FullName
    }
    $product = Get-CheckedEnum $productType 'Alphacam' 2
    $silent = Get-CheckedEnum $showType 'Silent' 2
    $openOk = Get-CheckedEnum $openResultType 'Ok' 1
    $validOk = Get-CheckedEnum $validResultType 'Ok' 101
    $setPath = Get-PublicMethod 'SetClsPath' @('System.String') 'System.Boolean'
    $sessionMethod = Get-PublicMethod 'GetCurrentSessionId' @() 'System.Int32'
    $openMethod = Get-PublicMethod 'OpenLicenseConnection' @(
        'System.String', $showType.FullName, $productType.FullName, 'System.Int32', 'System.Int32'
    ) $openResultType.FullName
    $validMethod = Get-PublicMethod 'ValidLicenseConnection' @($showType.FullName) $validResultType.FullName
    $productMethod = Get-PublicMethod 'GetClsProduct' @() $productType.FullName
    $closeMethod = Get-PublicMethod 'CloseLicenseConnection' @() 'System.Void'
    $state.runtime_contract_checked = $true
    Save-Phase 'before_set_cls_path'
    Assert-NoAcam
    $state.set_cls_path = [bool]$setPath.Invoke($null, [object[]]@($ClsRoot))
    if (-not $state.set_cls_path) { throw 'Original SetClsPath did not accept the installed CLS folder.' }
    $session = [int]$sessionMethod.Invoke($null, [object[]]@())
    $state.observed_session_id = $session
    $state.requested_product = [int]$product
    # Use the vendor's built-in check values, as the supplied Acam.exe does.
    # -1 is resolved inside the original DLL; it does not skip the check.
    $state.open_method = 'OpenLicenseConnection'
    $state.check_month = -1
    $state.check_year = -1
    $state.request_profile = 'installed_cls_defaults'
    $state.application_name = 'MiniCam CLS diagnostic'
    Save-Phase 'before_open'
    Assert-NoAcam
    $openAttempted = $true
    $opened = $openMethod.Invoke($null, [object[]]@(
        'MiniCam CLS diagnostic', $silent, $product, [int]-1, [int]-1
    ))
    $state.open_return_code = [int]$opened
    $state.open_return_name = [Enum]::GetName($opened.GetType(), $opened)
    Save-Phase 'after_open'
    Assert-NoAcam
    if ([int]$opened -ne [int]$openOk) { throw "CLS Open result: $($state.open_return_name) ($([int]$opened))" }
    Save-Phase 'before_validation'
    $valid = $validMethod.Invoke($null, [object[]]@($silent))
    $state.validation_return_code = [int]$valid
    $state.validation_return_name = [Enum]::GetName($valid.GetType(), $valid)
    $state.reported_product = [int]$productMethod.Invoke($null, [object[]]@())
    Save-Phase 'after_validation'
    if ([int]$valid -ne [int]$validOk -or $state.reported_product -ne [int]$product) {
        throw 'Current CLS session was not validated for the requested product.'
    }
    $state.cls_validated_before_translation = $true
    Assert-NoAcam
    $translator = $null
    try {
        Save-Phase 'before_com_activation'
        $comType = [Type]::GetTypeFromCLSID([Guid]$ClassId, $true)
        $translator = [Activator]::CreateInstance($comType)
        if ($null -eq $translator) { throw 'COM returned a null SolidLink object.' }
        $state.conversion.activated = $true
        Save-Phase 'after_com_activation'
        Assert-NoAcam
        $state.conversion.input_file_type = [int]$translator.GetFileType($InputModel)
        [void]$translator.SetOutputLogFile($true)
        Save-Phase 'before_translation'
        Assert-NoAcam
        # Normal public translation only. No license strings, access-code
        # fabrication, forced licensed flags or ILicense emulation.
        $state.step_conversion_attempted = $true
        $state.conversion.translation_attempted = $true
        Save-Phase 'translation_in_progress'
        $state.conversion.translate_return_code = [int]$translator.TranslateSolid(
            $InputModel, $OutputModel, 'Alphacam')
        Save-Phase 'after_translation'
        $state.conversion.interop_return_code = [int]$translator.GetInteropResult()
        $state.conversion.output_exists = [IO.File]::Exists($OutputModel)
        Save-Phase 'after_interop_result'
    } catch {
        $exception = $_.Exception
        if ($exception.InnerException) { $exception = $exception.InnerException }
        $state.conversion.error = @{
            type = $exception.GetType().FullName
            message = $exception.Message
        }
        Save-Phase 'conversion_error'
    } finally {
        if ($null -ne $translator) {
            Save-Phase 'before_com_release'
            try {
                $state.conversion.remaining_rcw_references = [Runtime.InteropServices.Marshal]::FinalReleaseComObject($translator)
                $state.conversion.com_release_completed = $true
            } catch {
                $state.conversion.cleanup_error = $_.Exception.Message
            }
            $translator = $null
            Save-Phase 'after_com_release'
        }
    }
    Assert-NoAcam
    Save-Phase 'before_validation_after_translation'
    $validAfter = $validMethod.Invoke($null, [object[]]@($silent))
    $state.validation_after_return_code = [int]$validAfter
    $state.validation_after_return_name = [Enum]::GetName($validAfter.GetType(), $validAfter)
    $state.reported_product_after = [int]$productMethod.Invoke($null, [object[]]@())
    Save-Phase 'after_validation_after_translation'
    if ([int]$validAfter -ne [int]$validOk -or $state.reported_product_after -ne [int]$product) {
        throw 'CLS session did not remain valid across the SolidLink trial.'
    }
    $state.cls_validated_after_translation = $true
    Assert-NoAcam
    # A completed diagnostic is distinct from a successful translation.
    # Python additionally requires TranslateSolid=0 and a new Parasolid file.
    $succeeded = $true
} catch {
    $exception = $_.Exception
    if ($exception.InnerException) { $exception = $exception.InnerException }
    $state.error = @{ type = $exception.GetType().FullName; message = $exception.Message }
    Save-Phase 'error'
} finally {
    if ($openAttempted -and $null -ne $closeMethod) {
        $state.close_attempted = $true
        Save-Phase 'before_close'
        try {
            # Current connection in this process only. Never close all sessions
            # or call ReleaseLicense/ReleaseAccessCode belonging to another app.
            [void]$closeMethod.Invoke($null, [object[]]@())
            $state.close_completed = $true
        } catch {
            $state.cleanup_error = $_.Exception.Message
            $succeeded = $false
        }
    }
    try { Assert-NoAcam } catch {
        $state.acam_check_error = $_.Exception.Message
        $succeeded = $false
    }
    if ($succeeded) { Save-Phase 'completed' } else { Save-Phase 'failed' }
}
if ($succeeded) { exit 0 } else { exit 1 }
