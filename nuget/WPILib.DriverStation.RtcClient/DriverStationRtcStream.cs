using System.Runtime.InteropServices;

namespace WPILib.DriverStation.RtcClient;

/// <summary>An asynchronous receive-only WHEP stream.</summary>
public sealed class DriverStationRtcStream : IDisposable
{
    private SafeStreamHandle? _handle;

    internal DriverStationRtcStream(SafeStreamHandle handle)
    {
        _handle = handle;
    }

    /// <summary>Gets the stream's current connection state.</summary>
    public DriverStationRtcStreamState State => NativeMethods.GetStreamState(GetHandle());

    /// <summary>Gets the latest asynchronous native error, or an empty string.</summary>
    public string Error
    {
        get
        {
            nint error = NativeMethods.GetStreamError(GetHandle());
            return error == 0 ? string.Empty : Marshal.PtrToStringUTF8(error) ?? string.Empty;
        }
    }

    /// <summary>Pauses decoding and frame delivery while keeping the WebRTC session alive.</summary>
    public void Pause()
    {
        SetPaused(paused: true);
    }

    /// <summary>Resumes decoding and requests a new H.264 keyframe.</summary>
    public void Resume()
    {
        SetPaused(paused: false);
    }

    /// <summary>
    /// Requests one new decoded bitmap while leaving the stream paused.
    /// </summary>
    /// <remarks>
    /// This method returns immediately. The native client requests a keyframe and
    /// decodes only until one bitmap is available through
    /// <see cref="TryGetNewestFrame"/>. Repeated calls are coalesced while a capture
    /// is already pending.
    /// </remarks>
    public void RequestFrame()
    {
        SafeStreamHandle handle = GetHandle();
        DriverStationRtcResult result = NativeMethods.RequestFrame(handle);
        if (result != DriverStationRtcResult.Success)
        {
            DriverStationRtc.ThrowIfFailed(result, GetError(handle));
        }
    }

    /// <summary>Pauses or resumes decoding and frame delivery.</summary>
    public void SetPaused(bool paused)
    {
        SafeStreamHandle handle = GetHandle();
        DriverStationRtcResult result = NativeMethods.SetStreamPaused(handle, paused ? 1 : 0);
        if (result != DriverStationRtcResult.Success)
        {
            DriverStationRtc.ThrowIfFailed(result, GetError(handle));
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

        SafeStreamHandle handle = GetHandle();
        DriverStationRtcResult result = NativeMethods.GetNewestFrame(handle, ref frame.NativeFrame);
        if (result == DriverStationRtcResult.NoFrame)
        {
            return false;
        }

        if (result != DriverStationRtcResult.Success)
        {
            DriverStationRtc.ThrowIfFailed(result, GetError(handle));
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
        SafeStreamHandle? handle = Interlocked.Exchange(ref _handle, null);
        handle?.Dispose();
    }

    private SafeStreamHandle GetHandle()
    {
        SafeStreamHandle? handle = Volatile.Read(ref _handle);
        ObjectDisposedException.ThrowIf(handle is null || handle.IsClosed, this);
        return handle;
    }

    private static string GetError(SafeStreamHandle handle)
    {
        nint error = NativeMethods.GetStreamError(handle);
        return error == 0 ? string.Empty : Marshal.PtrToStringUTF8(error) ?? string.Empty;
    }
}
