namespace WPILib.DriverStation.RtcClient;

/// <summary>
/// A reusable unmanaged buffer containing the newest decoded BGRA8888 bitmap.
/// </summary>
/// <remarks>
/// Dispose this object after use. Its buffer remains valid until the next successful
/// <see cref="DriverStationRtcStream.TryGetNewestFrame"/> call using this object, or disposal.
/// </remarks>
public sealed class DriverStationRtcFrame : IDisposable
{
    private NativeFrame _nativeFrame;
    private int _disposed;

    /// <summary>Gets whether this object currently contains a decoded bitmap.</summary>
    public bool HasData
    {
        get
        {
            ThrowIfDisposed();
            return _nativeFrame.Data != 0 && _nativeFrame.Length != 0;
        }
    }

    /// <summary>Gets the unmanaged pixel-buffer address.</summary>
    public nint Data
    {
        get
        {
            ThrowIfDisposed();
            return _nativeFrame.Data;
        }
    }

    /// <summary>Gets the number of valid bytes in <see cref="Data"/>.</summary>
    public nuint Length
    {
        get
        {
            ThrowIfDisposed();
            return _nativeFrame.Length;
        }
    }

    /// <summary>Gets the allocated capacity of the reusable native buffer.</summary>
    public nuint Capacity
    {
        get
        {
            ThrowIfDisposed();
            return _nativeFrame.Capacity;
        }
    }

    /// <summary>Gets the bitmap width in pixels.</summary>
    public uint Width
    {
        get
        {
            ThrowIfDisposed();
            return _nativeFrame.Width;
        }
    }

    /// <summary>Gets the bitmap height in pixels.</summary>
    public uint Height
    {
        get
        {
            ThrowIfDisposed();
            return _nativeFrame.Height;
        }
    }

    /// <summary>Gets the source row length in bytes.</summary>
    public uint Stride
    {
        get
        {
            ThrowIfDisposed();
            return _nativeFrame.Stride;
        }
    }

    /// <summary>Gets the decoded pixel format.</summary>
    public DriverStationRtcPixelFormat PixelFormat
    {
        get
        {
            ThrowIfDisposed();
            return _nativeFrame.PixelFormat;
        }
    }

    /// <summary>Gets the WebRTC frame timestamp in microseconds.</summary>
    public ulong TimestampMicroseconds
    {
        get
        {
            ThrowIfDisposed();
            return _nativeFrame.TimestampMicroseconds;
        }
    }

    /// <summary>Gets a view over the valid BGRA pixel bytes without copying.</summary>
    public unsafe ReadOnlySpan<byte> Pixels
    {
        get
        {
            ThrowIfDisposed();
            return _nativeFrame.Data == 0 || _nativeFrame.Length == 0
                ? ReadOnlySpan<byte>.Empty
                : new ReadOnlySpan<byte>((void*)_nativeFrame.Data, checked((int)_nativeFrame.Length));
        }
    }

    internal ref NativeFrame NativeFrame => ref _nativeFrame;

    /// <summary>Copies the tightly packed BGRA bytes into a managed destination.</summary>
    public void CopyTo(Span<byte> destination)
    {
        Pixels.CopyTo(destination);
    }

    /// <summary>Copies the bitmap row-by-row into a managed destination with its own stride.</summary>
    public void CopyTo(Span<byte> destination, int destinationStride)
    {
        ThrowIfDisposed();
        int height = checked((int)_nativeFrame.Height);
        int sourceStride = checked((int)_nativeFrame.Stride);
        ValidateDestination(destinationStride, height, sourceStride, destination.Length);

        ReadOnlySpan<byte> source = Pixels;
        for (int row = 0; row < height; ++row)
        {
            source.Slice(checked(row * sourceStride), sourceStride)
                .CopyTo(destination.Slice(checked(row * destinationStride), sourceStride));
        }
    }

    /// <summary>
    /// Copies the bitmap into an unmanaged destination with its own stride.
    /// </summary>
    /// <remarks>
    /// This can copy directly into an Avalonia locked framebuffer by passing its
    /// <c>Address</c> and <c>RowBytes</c>. The destination must contain at least
    /// <c>destinationStride * Height</c> writable bytes.
    /// </remarks>
    public unsafe void CopyTo(nint destination, int destinationStride)
    {
        ThrowIfDisposed();
        int height = checked((int)_nativeFrame.Height);
        int sourceStride = checked((int)_nativeFrame.Stride);
        if (height == 0)
        {
            return;
        }

        ArgumentOutOfRangeException.ThrowIfEqual(destination, 0);
        ValidateDestination(destinationStride, height, sourceStride, int.MaxValue);

        byte* source = (byte*)_nativeFrame.Data;
        byte* target = (byte*)destination;
        for (int row = 0; row < height; ++row)
        {
            Buffer.MemoryCopy(
                source + checked(row * sourceStride),
                target + checked(row * destinationStride),
                destinationStride,
                sourceStride);
        }
    }

    /// <inheritdoc />
    public void Dispose()
    {
        if (Interlocked.Exchange(ref _disposed, 1) == 0)
        {
            if (_nativeFrame.Data != 0)
            {
                NativeMethods.FreeFrame(ref _nativeFrame);
            }
            GC.SuppressFinalize(this);
        }
    }

    /// <summary>Releases the native frame allocation if the caller omitted disposal.</summary>
    ~DriverStationRtcFrame()
    {
        if (Interlocked.Exchange(ref _disposed, 1) == 0)
        {
            try
            {
                if (_nativeFrame.Data != 0)
                {
                    NativeMethods.FreeFrame(ref _nativeFrame);
                }
            }
            catch
            {
                // Finalizers must not surface native-loader failures during shutdown.
            }
        }
    }

    internal void ThrowIfDisposed()
    {
        ObjectDisposedException.ThrowIf(Volatile.Read(ref _disposed) != 0, this);
    }

    private static void ValidateDestination(
        int destinationStride,
        int height,
        int sourceStride,
        int destinationLength)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(destinationStride, sourceStride);
        int requiredLength = checked(destinationStride * height);
        if (destinationLength < requiredLength)
        {
            throw new ArgumentException(
                "The destination is too small for the decoded bitmap.",
                nameof(destinationLength));
        }
    }
}
