[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Directory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true

$resolvedDirectory = (Resolve-Path -LiteralPath $Directory).Path
$filesToSign = @(Get-ChildItem -LiteralPath $resolvedDirectory -File -Filter "*.dll")
if ($filesToSign.Count -eq 0) {
    throw "No DLLs were found in $resolvedDirectory."
}

Install-Module -Name TrustedSigning -Force -Scope CurrentUser -AllowClobber

foreach ($fileToSign in $filesToSign) {
    $existingSignature = Get-AuthenticodeSignature -LiteralPath $fileToSign.FullName
    if ($existingSignature.Status -eq [System.Management.Automation.SignatureStatus]::Valid) {
        Write-Host "$($fileToSign.Name) already has a valid signature; preserving it."
        continue
    }

    Invoke-TrustedSigning `
        -Endpoint $env:AZURE_SIGNING_ENDPOINT `
        -CodeSigningAccountName $env:AZURE_SIGNING_ACCOUNT_NAME `
        -CertificateProfileName $env:AZURE_SIGNING_PROFILE_NAME `
        -FileDigest SHA256 `
        -Files $fileToSign.FullName `
        -TimestampRfc3161 "http://timestamp.acs.microsoft.com" `
        -TimestampDigest SHA256

    $newSignature = Get-AuthenticodeSignature -LiteralPath $fileToSign.FullName
    if ($newSignature.Status -ne [System.Management.Automation.SignatureStatus]::Valid) {
        throw "Signing $($fileToSign.Name) did not produce a valid Authenticode signature: $($newSignature.StatusMessage)"
    }
}
