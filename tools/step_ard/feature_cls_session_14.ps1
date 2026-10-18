param([Parameter(Mandatory=$true)][string]$Root,
      [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$state=@{schema='feature-cls-session-14-v1';ready=$false;close_completed=$false;
         supported_special_key_mask=16;full_special_key_verified=$false;request_count=0}
$opened=$false;$granted=$false;$close=$null;$release=$null;$valid=$null;$quiet=$null
$utf8=New-Object System.Text.UTF8Encoding($false)
function Save-State([string]$name) {
    $text=ConvertTo-Json -InputObject $state -Depth 8
    $target=Join-Path $OutputDirectory $name
    $temporary=$target+'.writing'
    [IO.File]::WriteAllText($temporary,$text,$utf8)
    Move-Item -LiteralPath $temporary -Destination $target -Force
}
function Public-Method([Type]$type,[string]$name,[Type]$result,[Type[]]$arguments) {
    $m=$type.GetMethod($name,[Reflection.BindingFlags]'Public,Static',$null,$arguments,$null)
    if(-not $m -or $m.ReturnType -ne $result){throw ('Unexpected CLS method: '+$name)}
    return $m
}
function Invoke-Cls([Reflection.MethodInfo]$method,[object[]]$arguments) {
    $state.api_call=$method.Name
    return [FeatureClsInvocation]::Call($method,$arguments)
}
try {
    if([IntPtr]::Size -ne 8 -or [Environment]::Version.Major -ne 4){throw 'CLR4 x64 required'}
    if(@(Get-Process -Name Acam -ErrorAction SilentlyContinue).Count){throw 'Acam.exe must remain closed'}
    [string]$cls=Join-Path $Root 'CLS'
    $expected=@{
      'LicManaged.dll'='0b30282755e72feefdc3b43e50c0628749e49a5bd97fa06dabc002a0851a4827'
      'CLS.dll'='5d1e001654690774794b2d6093491e6b0b51aa670b6694f6c98883c29b8f02c8'
      'ClsInit.exe'='d594c321383c33ca4eb12eedf3f95cb2adfc269484cdea986fbe5973737ef252'
    }
    foreach($name in $expected.Keys){
        if((Get-FileHash -LiteralPath (Join-Path $cls $name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected[$name]){throw ('Unexpected CLS build: '+$name)}
    }
    $env:PATH=$cls+';'+$Root+';'+$env:PATH
    $type=[Reflection.Assembly]::LoadFrom((Join-Path $cls 'LicManaged.dll')).GetType('PlanitCls.License',$true)
    if(-not $type.IsPublic){throw 'CLS type is not public'}
    $productType=$type.GetNestedType('Product',[Reflection.BindingFlags]::Public)
    $showType=$type.GetNestedType('ShowError',[Reflection.BindingFlags]::Public)
    $openType=$type.GetNestedType('OpenLicenseConnectionResult',[Reflection.BindingFlags]::Public)
    $validType=$type.GetNestedType('ValidLicenseConnectionResult',[Reflection.BindingFlags]::Public)
    $releaseType=$type.GetNestedType('ReleaseAccessCodeResult',[Reflection.BindingFlags]::Public)
    $product=[Enum]::Parse($productType,'Alphacam');$quiet=[Enum]::Parse($showType,'Silent')
    if([int]$product -ne 2 -or [int]$quiet -ne 2 -or [int][Enum]::Parse($openType,'Ok') -ne 1 -or [int][Enum]::Parse($validType,'Ok') -ne 101){throw 'Unexpected CLS enum values'}
    $set=Public-Method $type 'SetClsPath' ([bool]) ([Type[]]@([string]))
    $open=Public-Method $type 'OpenLicenseConnection' $openType ([Type[]]@([string],$showType,$productType,[int],[int]))
    $valid=Public-Method $type 'ValidLicenseConnection' $validType ([Type[]]@($showType))
    $close=Public-Method $type 'CloseLicenseConnection' ([void]) ([Type[]]@())
    $release=Public-Method $type 'ReleaseAccessCode' $releaseType ([Type[]]@([int]))
    $access=Public-Method $type 'GetAccessCode' ([bool]) ([Type[]]@([int]))
    $state.api_call='argument_bridge_compile'
    Add-Type -Path (Join-Path $PSScriptRoot 'feature_cls_invocation.cs') -ReferencedAssemblies @('System.dll','System.Core.dll',[System.Management.Automation.PSObject].Assembly.Location)
    $state.argument_bridge_selftest=[FeatureClsInvocation]::SelfTest()
    if(-not $state.argument_bridge_selftest){throw 'Reflection argument bridge self-test failed'}
    if(-not (Invoke-Cls $set ([object[]]@([string]$cls)))){throw 'SetClsPath failed'}
    $opened=$true
    $state.open_return_code=[int](Invoke-Cls $open ([object[]]@('MiniCam Feature writer diagnostic',$quiet,$product,[int]-1,[int]-1)))
    if($state.open_return_code -ne 1){throw 'CLS connection refused'}
    $state.validation_before=[int](Invoke-Cls $valid ([object[]]@($quiet)))
    if($state.validation_before -ne 101){throw 'CLS invalid before classification'}
    $state.request_count=1;$state.access_code=348
    $granted=[bool](Invoke-Cls $access ([object[]]@([int]348)))
    $state.classification_flag=$granted
    $state.special_key_projection=0
    if($granted){$state.special_key_projection=16}
    $state.validation_after=[int](Invoke-Cls $valid ([object[]]@($quiet)))
    if($state.validation_after -ne 101){throw 'CLS invalid after classification'}
    $state.ready=$true
    Save-State 'cls_ready.json'
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while(-not (Test-Path -LiteralPath (Join-Path $OutputDirectory 'feature_done.signal'))){
        if($timer.Elapsed.TotalSeconds -gt 150){$state.lease_expired=$true;throw 'Feature session wait expired'}
        if([int](Invoke-Cls $valid ([object[]]@($quiet))) -ne 101){throw 'CLS became invalid during Feature stage'}
        Start-Sleep -Milliseconds 200
    }
    $state.validation_after_feature=[int](Invoke-Cls $valid ([object[]]@($quiet)))
    if($state.validation_after_feature -ne 101){throw 'CLS invalid after Feature'}
} catch {
    $state.error_type=$_.Exception.GetType().FullName
    $state.error_message=$_.Exception.Message
    $state.error_line=$_.InvocationInfo.ScriptLineNumber
    if($state.ContainsKey('api_call')){$state.failed_api_call=$state.api_call}
    if($_.Exception.InnerException){$state.inner_error_message=$_.Exception.InnerException.Message}
} finally {
    if($granted -and $release){
        try {
            $result=Invoke-Cls $release ([object[]]@([int]348))
            $state.release_return_code=[int]$result;$state.release_return_name=$result.ToString()
            $state.individual_release_confirmed=([int]$result -eq 201 -or [int]$result -eq 207)
            if([int]$result -notin @(201,202,207)){$state.release_error=$true}
        } catch {$state.release_error=$true;$state.release_error_message=$_.Exception.Message}
    }
    if($opened -and $close){
        try {$null=Invoke-Cls $close ([object[]]@());$state.close_completed=$true}
        catch {$state.close_error=$true;$state.close_error_message=$_.Exception.Message}
    }
    Save-State 'cls_result.json'
}
if($state.ContainsKey('error_type') -or $state.ContainsKey('release_error') -or $state.ContainsKey('close_error') -or -not $state.close_completed){exit 1}
exit 0
