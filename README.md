# DriverStationRtcClient

Native C ABI client for the FIRST driver station H.264/WebRTC integration.
The build embeds static Mbed TLS 3.6.7 and libdatachannel libraries into a
shared `DriverStationRtc` library and downloads the Cisco OpenH264 2.6.0
runtime for the active platform. Only the symbols listed in `exports.txt` are
public.

## Streaming API

Call `DriverStationRtc_StartModule()` once, then pass a Limelight WHEP endpoint
such as `http://limelight.local:5807/whep` to
`DriverStationRtc_StartStream()`. Stream creation is asynchronous; the opaque
handle can be stored in a .NET `SafeHandle`, and
`DriverStationRtc_GetStreamState()` reports connection progress or failure.
The start-time callback runs after every decoded bitmap is published and also
reports asynchronous stream errors.

The receive track is restricted to H.264. Complete RTP access units are
depacketized by libdatachannel, decoded by OpenH264, and converted from I420 to
opaque BGRA8888. `DriverStationRtc_GetNewestFrame()` returns only the newest
bitmap and returns `DRIVER_STATION_RTC_NO_FRAME` until a newer bitmap arrives.
Its `DriverStationRtcFrame` reports width, height, stride, length, capacity, and
timestamp. The buffer can be reused on later calls or released with
`DriverStationRtc_FreeFrame()`; its BGRA byte order can be copied directly into
an Avalonia `WriteableBitmap` created with `PixelFormat.Bgra8888` and
`AlphaFormat.Opaque`.

The frame callback can call `DriverStationRtc_GetNewestFrame()` immediately
after receiving `DRIVER_STATION_RTC_SUCCESS`. Stopping the stream disables the
callback and waits for a callback that is already running, so no callback runs
after stop returns.
Stop each handle with `DriverStationRtc_StopStream()`, or stop every stream and
the global WebRTC resources with `DriverStationRtc_StopModule()`.

## .NET wrapper

The `WPILib.DriverStation.RtcClient` NuGet package targets .NET 10, uses
source-generated `LibraryImport` bindings, and is marked AOT-compatible. It
contains the managed wrapper and both native libraries for every supported RID.

```csharp
using WPILib.DriverStation.RtcClient;

DriverStationRtc.StartModule();
using var frame = new DriverStationRtcFrame();

using var stream = DriverStationRtc.StartStream(
    "http://limelight.local:5807/whep",
    (activeStream, result) =>
{
    if (result == DriverStationRtcResult.Success
        && activeStream.TryGetNewestFrame(frame))
    {
        using var framebuffer = writeableBitmap.Lock();
        frame.CopyTo(framebuffer.Address, framebuffer.RowBytes);
    }
});
// The callback runs for every decoded frame until the stream is stopped.
stream.Stop();
DriverStationRtc.StopModule();
```

Create the Avalonia `WriteableBitmap` with `PixelFormat.Bgra8888` and
`AlphaFormat.Opaque`. A `DriverStationRtcFrame` owns a reusable unmanaged
buffer; subsequent successful calls grow or reuse it, and disposal releases it.

The managed smoke application runs its offline lifecycle checks with no
arguments. Pass either a camera IP or a complete WHEP URL to wait up to 20
seconds for three decoded frames:

```powershell
dotnet run --project tests/managed/DriverStationRtc.ManagedSmoke.csproj -- 192.168.5.220
```

## Windows build

Visual Studio Build Tools, CMake, Ninja, and Python 3.8 or newer must be
installed. The checked-in PowerShell entry point locates the latest Visual
Studio installation, enters an MSVC developer environment, configures the Ninja
build, and runs the link smoke test:

```powershell
./scripts/Build-Windows.ps1
```

Use `-Arch arm64` for a Windows ARM64 build. All project-built libraries use the
static MSVC runtime (`/MT`, or `/MTd` for Debug). OpenH264 is Cisco's signed
prebuilt runtime and is copied beside the generated DLL and smoke-test program.
The DLL is linked with `/DEPENDENTLOADFLAG:0x1100`, which enables
`LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS` so its
OpenH264 dependency is resolved from the DLL's directory.

## Linux and macOS builds

CMake and Ninja are required. Native presets configure, build, and test each
platform:

```sh
cmake --preset linux       # Use macos on macOS
cmake --build --preset linux
ctest --preset linux
```

The shared library in the build directory resolves the versioned OpenH264
runtime only from its own directory (`$ORIGIN` on Linux and `@loader_path` on
macOS). The build copies that runtime beside the library, so running CMake's
install step is not required.

## Continuous integration

GitHub Actions builds Linux x64, Linux ARM64, Windows x64, Windows ARM64,
macOS ARM64, and macOS x64 with Ninja. All native jobs run the dependency smoke
test; Windows ARM64 is cross-compiled and link-checked on the Windows x64
runner. CI inspects each binary's loader metadata and publishes the raw
build-directory binaries rather than a CMake install tree.

CI also creates the `WPILib.DriverStation.RtcClient` NuGet package. It contains
the .NET 10 AOT-compatible wrapper, and each supported runtime identifier
contains both `DriverStationRtc` and its matching OpenH264 binary in the same
`runtimes/<rid>/native` directory. The package contains the aggregate
`LICENSE.txt`, including all dependency license terms, and intentionally
contains no static libraries or Windows import libraries. Release signing and
Artifactory publishing are checked in as disabled workflow steps so normal CI
can be proven before credentials and external publishing are enabled.

## License

DriverStationRtcClient is licensed under the BSD 3-Clause license. See
`LICENSE` for the project terms and `THIRD_PARTY_NOTICES.md` for the dependency
inventory. The raw build directory (and optional installed package) contains a
single aggregate `LICENSE.txt` with the full project and third-party license
texts, including Cisco's OpenH264 binary and AVC/H.264 patent-license notice.
