[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackagePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$resolvedPackage = (Resolve-Path -LiteralPath $PackagePath).Path
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::OpenRead($resolvedPackage)

try {
    $entries = @($archive.Entries | ForEach-Object { $_.FullName.TrimStart('/') })
    $expectedEntries = @(
        "lib/net10.0/WPILib.DriverStation.RtcClient.dll",
        "lib/net10.0/WPILib.DriverStation.RtcClient.xml",
        "runtimes/win-x64/native/DriverStationRtc.dll",
        "runtimes/win-x64/native/openh264.dll",
        "runtimes/win-arm64/native/DriverStationRtc.dll",
        "runtimes/win-arm64/native/openh264.dll",
        "runtimes/linux-x64/native/libDriverStationRtc.so",
        "runtimes/linux-x64/native/libopenh264.so.8",
        "runtimes/linux-arm64/native/libDriverStationRtc.so",
        "runtimes/linux-arm64/native/libopenh264.so.8",
        "runtimes/osx-x64/native/libDriverStationRtc.dylib",
        "runtimes/osx-x64/native/libopenh264.8.dylib",
        "runtimes/osx-arm64/native/libDriverStationRtc.dylib",
        "runtimes/osx-arm64/native/libopenh264.8.dylib",
        "LICENSE.txt",
        "THIRD_PARTY_NOTICES.md",
        "README.md"
    )

    foreach ($expectedEntry in $expectedEntries) {
        if ($entries -notcontains $expectedEntry) {
            throw "NuGet package is missing $expectedEntry."
        }
    }

    $expectedNativeEntries = @($expectedEntries | Where-Object { $_.StartsWith("runtimes/") })
    $actualNativeEntries = @($entries | Where-Object { $_.StartsWith("runtimes/") })
    $unexpectedNativeEntries = @($actualNativeEntries | Where-Object { $expectedNativeEntries -notcontains $_ })
    if ($unexpectedNativeEntries.Count -ne 0) {
        throw "The package contains unexpected runtime files: $($unexpectedNativeEntries -join ', ')"
    }

    $staticLibraries = @($entries | Where-Object { $_ -match '\.(a|lib)$' })
    if ($staticLibraries.Count -ne 0) {
        throw "The package contains static/import libraries: $($staticLibraries -join ', ')"
    }

    $runtimeSuffixEntries = @($entries | Where-Object { $_ -match 'RtcClient\.runtime' })
    if ($runtimeSuffixEntries.Count -ne 0) {
        throw "The package still contains runtime-suffixed artifacts: $($runtimeSuffixEntries -join ', ')"
    }

    $nuspecEntry = $archive.Entries | Where-Object { $_.FullName -like "*.nuspec" }
    $nuspecReader = [System.IO.StreamReader]::new($nuspecEntry.Open())
    try {
        $nuspecText = $nuspecReader.ReadToEnd()
    }
    finally {
        $nuspecReader.Dispose()
    }
    if ($nuspecText -notmatch '<id>WPILib\.DriverStation\.RtcClient</id>') {
        throw "The NuGet package ID does not match WPILib.DriverStation.RtcClient."
    }

    $licenseEntry = $archive.Entries | Where-Object { $_.FullName -eq "LICENSE.txt" }
    $reader = [System.IO.StreamReader]::new($licenseEntry.Open())
    try {
        $licenseText = $reader.ReadToEnd()
    }
    finally {
        $reader.Dispose()
    }

    foreach ($dependencyName in @("Mbed TLS", "libdatachannel", "OpenH264")) {
        if ($licenseText -notmatch [regex]::Escape($dependencyName)) {
            throw "The packaged LICENSE.txt does not contain the $dependencyName license section."
        }
    }
}
finally {
    $archive.Dispose()
}

Write-Host "Validated NuGet package: $resolvedPackage"
