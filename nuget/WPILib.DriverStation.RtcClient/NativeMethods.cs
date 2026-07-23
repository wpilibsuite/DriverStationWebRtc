using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace WPILib.DriverStation.RtcClient;

[StructLayout(LayoutKind.Sequential)]
internal struct NativeFrame
{
    internal nint Data;
    internal nuint Length;
    internal nuint Capacity;
    internal uint Width;
    internal uint Height;
    internal uint Stride;
    internal DriverStationRtcPixelFormat PixelFormat;
    internal ulong TimestampMicroseconds;
}

internal sealed class SafeStreamHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    private SafeStreamHandle()
        : base(ownsHandle: true)
    {
    }

    internal SafeStreamHandle(nint stream)
        : base(ownsHandle: true)
    {
        SetHandle(stream);
    }

    protected override bool ReleaseHandle()
    {
        try
        {
            DriverStationRtcResult result = NativeMethods.StopStream(handle);
            return result is DriverStationRtcResult.Success or DriverStationRtcResult.InvalidArgument;
        }
        catch
        {
            return false;
        }
    }
}

internal static partial class NativeMethods
{
    private const string LibraryName = "DriverStationRtc";

    [LibraryImport(LibraryName, EntryPoint = "DriverStationRtc_StartModule")]
    internal static partial DriverStationRtcResult StartModule();

    [LibraryImport(LibraryName, EntryPoint = "DriverStationRtc_StopModule")]
    internal static partial DriverStationRtcResult StopModule();

    [LibraryImport(
        LibraryName,
        EntryPoint = "DriverStationRtc_StartStream",
        StringMarshalling = StringMarshalling.Utf8)]
    internal static unsafe partial DriverStationRtcResult StartStream(
        string url,
        delegate* unmanaged[Cdecl]<DriverStationRtcResult, nint, void> callback,
        nint userData,
        out nint stream);

    [LibraryImport(LibraryName, EntryPoint = "DriverStationRtc_StopStream")]
    internal static partial DriverStationRtcResult StopStream(nint stream);

    [LibraryImport(LibraryName, EntryPoint = "DriverStationRtc_GetStreamState")]
    internal static partial DriverStationRtcStreamState GetStreamState(SafeStreamHandle stream);

    [LibraryImport(LibraryName, EntryPoint = "DriverStationRtc_GetStreamError")]
    internal static partial nint GetStreamError(SafeStreamHandle stream);

    [LibraryImport(LibraryName, EntryPoint = "DriverStationRtc_GetNewestFrame")]
    internal static partial DriverStationRtcResult GetNewestFrame(
        SafeStreamHandle stream,
        ref NativeFrame frame);

    [LibraryImport(LibraryName, EntryPoint = "DriverStationRtc_FreeFrame")]
    internal static partial void FreeFrame(ref NativeFrame frame);
}
