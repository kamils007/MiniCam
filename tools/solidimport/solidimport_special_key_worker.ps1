param(
    [Parameter(Mandatory=$true)][string]$Root,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
try {
    if ([IntPtr]::Size -ne 8 -or [Environment]::Version.Major -ne 4) {
        throw 'Windows PowerShell x64 / CLR v4 required.'
    }
    if (@(Get-Process -Name 'Acam' -ErrorAction SilentlyContinue).Count -gt 0) {
        throw 'Acam.exe must remain closed.'
    }
    $Root = [IO.Path]::GetFullPath($Root)
    $env:PATH = (Join-Path $Root 'CLS') + ';' + $Root + ';' + $env:PATH
    $sources = @(
        (Join-Path $PSScriptRoot 'cls_license_adapter.cs'),
        (Join-Path $PSScriptRoot 'solidimport_special_key_host.cs'),
        (Join-Path $PSScriptRoot 'solidimport_special_key_native.cs')
    )
    Add-Type -Path $sources -ReferencedAssemblies 'System.dll','System.Core.dll','System.Web.Extensions.dll'
    [MiniCamSolidImportBridge]::Run($Root, $OutputDirectory)
    throw 'Terminal native diagnostic unexpectedly returned.'
} catch {
    $exception = $_.Exception
    if ($exception.InnerException) { $exception = $exception.InnerException }
    $report = @{ schema='solidimport-special-key-bootstrap-error-v1'; phase='bootstrap_failed';
                 error_type=$exception.GetType().FullName; error_message=$exception.Message }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'bootstrap_error.json'),
        (ConvertTo-Json -InputObject $report -Depth 4), (New-Object System.Text.UTF8Encoding($false)))
    exit 1
}
