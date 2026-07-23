using System.Runtime.InteropServices;

namespace WPILib.DriverStation.RtcClient;

/// <summary>An asynchronous receive-only WHEP stream.</summary>
public sealed class DriverStationRtcStream : IDisposable
{
    private readonly Action cancelFrameCallback;
    private SafeStreamHandle? handle;

    internal DriverStationRtcStream(
        SafeStreamHandle handle,
        Action cancelFrameCallback)
    {
        this.handle = handle;
        this.cancelFrameCallback = cancelFrameCallback;
    }

    /// <summary>Gets the stream's current connection state.</summary>
    public DriverStationRtcStreamState State => NativeMethods.GetStreamState(GetHandle());

    /// <summary>Gets the latest asynchronous native error, or an empty string.</summary>
    public string Error
    {
        get
        {
            var error = NativeMethods.GetStreamError(GetHandle());
            return error == 0 ? string.Empty : Marshal.PtrToStringUTF8(error) ?? string.Empty;
        }
    }

    /// <summary>
    /// Copies the newest decoded bitmap into a reusable frame buffer.
    /// </summary>
    /// <returns>
    /// <see langword="true"/> when a newer frame was copied; otherwise
    /// <see langword="false"/> when no frame has arrived since the last successful call.
    /// </returns>
    public bool TryGetNewestFrame(DriverStationRtcFrame frame)
    {
        ArgumentNullException.ThrowIfNull(frame);
        frame.ThrowIfDisposed();

        var activeHandle = GetHandle();
        var result = NativeMethods.GetNewestFrame(activeHandle, ref frame.NativeFrame);
        if (result == DriverStationRtcResult.NoFrame)
        {
            return false;
        }

        if (result != DriverStationRtcResult.Success)
        {
            DriverStationRtc.ThrowIfFailed(result, GetError(activeHandle));
        }
        return true;
    }

    /// <summary>Stops the stream and releases its native resources.</summary>
    public void Stop()
    {
        Dispose();
    }

    /// <inheritdoc />
    public void Dispose()
    {
        var activeHandle = Interlocked.Exchange(ref handle, null);
        activeHandle?.Dispose();
        cancelFrameCallback();
    }

    private SafeStreamHandle GetHandle()
    {
        var activeHandle = Volatile.Read(ref handle);
        ObjectDisposedException.ThrowIf(
            activeHandle is null || activeHandle.IsClosed,
            this);
        return activeHandle;
    }

    private static string GetError(SafeStreamHandle handle)
    {
        var error = NativeMethods.GetStreamError(handle);
        return error == 0 ? string.Empty : Marshal.PtrToStringUTF8(error) ?? string.Empty;
    }
}
