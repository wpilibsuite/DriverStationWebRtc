using System.Collections.Concurrent;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace WPILib.DriverStation.RtcClient;

/// <summary>Controls the process-wide DriverStationRtc module.</summary>
public static class DriverStationRtc
{
    private static readonly ConcurrentDictionary<nint, WeakReference<FrameRegistration>>
        frameRegistrations = new();
    private static long nextFrameContext;

    /// <summary>Starts the process-wide WebRTC resources. This method is idempotent.</summary>
    public static void StartModule()
    {
        ThrowIfFailed(NativeMethods.StartModule());
    }

    /// <summary>
    /// Stops all streams and the process-wide WebRTC resources. This method is idempotent.
    /// </summary>
    public static void StopModule()
    {
        ThrowIfFailed(NativeMethods.StopModule());
    }

    /// <summary>Starts an asynchronous receive-only WHEP stream.</summary>
    /// <param name="url">
    /// An HTTP WHEP endpoint, such as <c>http://limelight.local:5807/whep</c>.
    /// </param>
    /// <param name="callback">
    /// Called after each decoded frame is published, or when an asynchronous stream
    /// error occurs. A successful callback can retrieve the frame through
    /// <see cref="DriverStationRtcStream.TryGetNewestFrame"/>.
    /// </param>
    /// <returns>A disposable stream whose state can be polled while it connects.</returns>
    public static DriverStationRtcStream StartStream(
        string url,
        Action<DriverStationRtcStream, DriverStationRtcResult> callback)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(url);
        ArgumentNullException.ThrowIfNull(callback);

        var registration = new FrameRegistration(callback);
        var result = StartStreamNative(url, registration.Context, out var stream);
        if (result != DriverStationRtcResult.Success)
        {
            registration.Cancel();
            if (stream != 0)
            {
                NativeMethods.StopStream(stream);
            }

            ThrowIfFailed(result);
        }
        if (stream == 0)
        {
            registration.Cancel();
            throw new DriverStationRtcException(
                DriverStationRtcResult.InternalError,
                "The native library returned a null stream handle.");
        }

        var managedStream = new DriverStationRtcStream(
            new SafeStreamHandle(stream),
            registration.Cancel);
        registration.Attach(managedStream);
        return managedStream;
    }

    internal static void ThrowIfFailed(DriverStationRtcResult result, string? detail = null)
    {
        if (result != DriverStationRtcResult.Success)
        {
            throw new DriverStationRtcException(result, detail);
        }
    }

    private static unsafe DriverStationRtcResult StartStreamNative(
        string url,
        nint context,
        out nint stream)
    {
        return NativeMethods.StartStream(url, &OnFrameAvailable, context, out stream);
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void OnFrameAvailable(DriverStationRtcResult result, nint context)
    {
        try
        {
            if (frameRegistrations.TryGetValue(context, out var weakRegistration)
                && weakRegistration.TryGetTarget(out var registration))
            {
                registration.Notify(result);
            }
        }
        catch
        {
            // Managed exceptions must not cross the native callback boundary.
        }
    }

    private sealed class FrameRegistration
    {
        private readonly object sync = new();
        private readonly Queue<DriverStationRtcResult> pendingResults = new();
        private readonly Action<DriverStationRtcStream, DriverStationRtcResult> callback;
        private readonly nint context;
        private WeakReference<DriverStationRtcStream>? stream;
        private bool draining;
        private bool canceled;

        internal FrameRegistration(
            Action<DriverStationRtcStream, DriverStationRtcResult> callback)
        {
            this.callback = callback;
            context = Register(this);
        }

        internal nint Context => context;

        internal void Attach(DriverStationRtcStream owner)
        {
            var shouldDrain = false;
            lock (sync)
            {
                if (canceled)
                {
                    return;
                }
                stream = new WeakReference<DriverStationRtcStream>(owner);
                if (!draining && pendingResults.Count != 0)
                {
                    draining = true;
                    shouldDrain = true;
                }
            }

            if (shouldDrain)
            {
                Drain();
            }
        }

        internal void Notify(DriverStationRtcResult result)
        {
            var shouldDrain = false;
            lock (sync)
            {
                if (canceled)
                {
                    return;
                }
                pendingResults.Enqueue(result);
                if (!draining && stream is not null)
                {
                    draining = true;
                    shouldDrain = true;
                }
            }

            if (shouldDrain)
            {
                Drain();
            }
        }

        internal void Cancel()
        {
            lock (sync)
            {
                if (canceled)
                {
                    return;
                }
                canceled = true;
                pendingResults.Clear();
                stream = null;
            }
            frameRegistrations.TryRemove(context, out _);
            GC.SuppressFinalize(this);
        }

        private void Drain()
        {
            while (true)
            {
                DriverStationRtcStream? owner;
                DriverStationRtcResult result;
                lock (sync)
                {
                    if (canceled
                        || stream is null
                        || !stream.TryGetTarget(out owner)
                        || owner is null
                        || pendingResults.Count == 0)
                    {
                        draining = false;
                        return;
                    }
                    result = pendingResults.Dequeue();
                }

                try
                {
                    callback(owner, result);
                }
                catch
                {
                    // User callbacks must not escape into the native callback thread.
                }
            }
        }

        ~FrameRegistration()
        {
            frameRegistrations.TryRemove(context, out _);
        }

        private static nint Register(FrameRegistration registration)
        {
            while (true)
            {
                var candidate = unchecked((nint)Interlocked.Increment(ref nextFrameContext));
                if (candidate != 0
                    && frameRegistrations.TryAdd(
                        candidate,
                        new WeakReference<FrameRegistration>(registration)))
                {
                    return candidate;
                }
            }
        }
    }
}
