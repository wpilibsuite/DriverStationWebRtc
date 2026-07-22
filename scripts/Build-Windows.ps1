[CmdletBinding()]
param(
    [ValidateSet("x64", "arm64")]
    [string]$Arch = "x64",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "RelWithDebInfo",
    [switch]$SkipTests
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true

$targetArch = if ($Arch -eq "x64") { "amd64" } else { "arm64" }
& "$PSScriptRoot/Launch-VsDevShell.ps1" -Latest -Arch $targetArch -HostArch amd64 -SkipAutomaticLocation

if (-not $env:DevEnvDir) {
    throw "Visual Studio Developer PowerShell could not be initialized."
}

$sourceDir = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $sourceDir "build/windows-$Arch"

$configureArgs = @(
    "-S", $sourceDir,
    "-B", $buildDir,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded"
)

& cmake @configureArgs
& cmake --build $buildDir
if (-not $SkipTests) {
    & ctest --test-dir $buildDir --output-on-failure
}
