param(
    [Parameter(Mandatory=$true)][string]$ClsRoot,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [switch]$ProbeSolidsAccess,
    [switch]$ProbeLicenseAdapter,
    [string]$AcamExecutable
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$state = [ordered]@{
    schema = 'cls-managed-worker-v2'
    phase = 'start'
    runtime_contract_checked = $false
    core_solids_verified = $false
    step_conversion_attempted = $false
    release_version_verified = $false
    close_attempted = $false
    close_completed = $false
}
$utf8 = New-Object System.Text.UTF8Encoding($false)
$resultPath = Join-Path $OutputDirectory 'worker_result.json'
$checkpointsPath = Join-Path $OutputDirectory 'worker_checkpoints.jsonl'
$openAttempted = $false
$closeMethod = $null
$succeeded = $false
$featureGranted = $false
$releaseFeatureMethod = $null

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
    if ($ProbeLicenseAdapter -and -not $ProbeSolidsAccess) {
        throw 'The adapter probe requires the checked solids-access profile.'
    }
    if ($ProbeSolidsAccess) {
        $acamHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $AcamExecutable).Hash.ToLowerInvariant()
        if ($acamHash -ne '02997d0f6c15d0e6c807d1be19621e128bdbf3a55ae1846e0196872766bf205d') {
            throw 'The installed Acam.exe does not match the analyzed 2025.4.1.104 build.'
        }
        $state.source_acam_sha256 = $acamHash
        $state.feature = [ordered]@{
            requested_code = 390
            scope = 'One original CLS access-code request; not the complete Core:Solids host policy'
            api_contract_checked = $false
            checkout_attempted = $false
            checkout_granted = $false
            release_attempted = $false
            release_completed = $false
            validated_after_request = $false
        }
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
    if ($ProbeSolidsAccess) {
        $featureMethod = Get-PublicMethod 'GetAccessCode' @('System.Int32') 'System.Boolean'
        $releaseType = Get-PublicLicenseEnum 'ReleaseAccessCodeResult'
        [void](Get-CheckedEnum $releaseType 'Ok' 201)
        [void](Get-CheckedEnum $releaseType 'CapacityReleasedOk' 207)
        $releaseFeatureMethod = Get-PublicMethod 'ReleaseAccessCode' @('System.Int32') $releaseType.FullName
        $state.feature.api_contract_checked = $true
        if ($ProbeLicenseAdapter) {
            Save-Phase 'before_adapter_compile'
            Add-Type -Path (Join-Path $PSScriptRoot 'cls_license_adapter.cs') -ReferencedAssemblies 'System.dll','System.Core.dll'
            $state.license_adapter = [ordered]@{ compiled = $true; native_roundtrip_verified = $false }
        }
    }
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
    Assert-NoAcam
    if ($ProbeSolidsAccess) {
        Save-Phase 'before_feature_request'
        $state.feature.checkout_attempted = $true
        Save-Phase 'feature_request_in_progress'
        # Exactly the CLS feature request reached by the supplied Acam.exe's
        # non-cached Core:Solids path. No other access codes are tried.
        if ($ProbeLicenseAdapter) {
            Save-Phase 'before_adapter_native_call'
            $state.license_adapter = [MiniCamClsLicenseNativeProbe]::Run($licenseType, $silent)
            $featureGranted = [bool]$state.license_adapter.checkout_granted
            $state.feature.checkout_attempted = [bool]$state.license_adapter.checkout_attempted
        } else {
            $featureGranted = [bool]$featureMethod.Invoke($null, [object[]]@([int]390))
        }
        $state.feature.checkout_granted = $featureGranted
        Save-Phase 'after_feature_request'
        Assert-NoAcam
        $after = $validMethod.Invoke($null, [object[]]@($silent))
        $state.feature.validation_after_return_code = [int]$after
        $state.feature.validated_after_request = ([int]$after -eq [int]$validOk)
        if (-not $state.feature.validated_after_request) { throw 'CLS validation failed after the feature request.' }
        if ($ProbeLicenseAdapter -and -not $state.license_adapter.native_roundtrip_verified) {
            throw 'The native ILicense adapter round trip was not verified.'
        }
    }
    $succeeded = $true
} catch {
    $exception = $_.Exception
    if ($exception.InnerException) { $exception = $exception.InnerException }
    $state.error = @{ type = $exception.GetType().FullName; message = $exception.Message }
    Save-Phase 'error'
} finally {
    if ($featureGranted -and $null -ne $releaseFeatureMethod) {
        $state.feature.release_attempted = $true
        Save-Phase 'before_feature_release'
        try {
            # Release only the successful request made by this worker.
            $released = $releaseFeatureMethod.Invoke($null, [object[]]@([int]390))
            $state.feature.release_return_code = [int]$released
            $state.feature.release_return_name = [Enum]::GetName($released.GetType(), $released)
            $state.feature.release_completed = ([int]$released -eq 201 -or [int]$released -eq 207)
            $notOptional = ([int]$released -eq 202 -and $state.feature.release_return_name -eq 'NotOptional')
            if ($ProbeLicenseAdapter) { $state.feature.release_not_optional = $notOptional }
            # Test 9 preserves NotOptional separately; it never calls it a release.
            # Test 8 retains its original strict whole-test criterion.
            if (-not $state.feature.release_completed -and -not ($ProbeLicenseAdapter -and $notOptional)) {
                $succeeded = $false
            }
        } catch {
            $state.feature.release_error = $_.Exception.Message
            $succeeded = $false
        }
    }
    if ($openAttempted -and $null -ne $closeMethod) {
        $state.close_attempted = $true
        Save-Phase 'before_close'
        try {
            # Current connection in this process only. Never close all sessions
            # or release requests belonging to another app.
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
