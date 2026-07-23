using WPILib.DriverStation.RtcClient;

return args.Length == 0 ? RunOfflineSmoke() : RunLiveStream(args[0]);

static int RunOfflineSmoke()
{
    DriverStationRtc.StartModule();
    try
    {
        var callbackCount = 0;
        using var stream = DriverStationRtc.StartStream(
            "https://unsupported.invalid/whep",
            (_, _) => Interlocked.Increment(ref callbackCount));
        using var frame = new DriverStationRtcFrame();

        Check(!stream.TryGetNewestFrame(frame), "A new stream unexpectedly returned a frame.");
        Check(!frame.HasData, "The empty reusable frame buffer reports pixel data.");
        Check(frame.Pixels.IsEmpty, "The empty reusable frame buffer exposes pixel bytes.");

        stream.Stop();
        var callbackCountAfterStop = Volatile.Read(ref callbackCount);
        Thread.Sleep(50);
        Check(
            Volatile.Read(ref callbackCount) == callbackCountAfterStop,
            "A frame callback ran after its stream was stopped.");

        var disposedStreamRejected = false;
        try
        {
            _ = stream.State;
        }
        catch (ObjectDisposedException)
        {
            disposedStreamRejected = true;
        }

        Check(disposedStreamRejected, "A stopped managed stream remained usable.");
        return 0;
    }
    finally
    {
        DriverStationRtc.StopModule();
    }
}

static int RunLiveStream(string endpointOrIp)
{
    var endpoint = NormalizeEndpoint(endpointOrIp);
    Console.WriteLine($"Connecting to {endpoint}");

    DriverStationRtc.StartModule();
    try
    {
        using var frame = new DriverStationRtcFrame();
        using var completed = new ManualResetEventSlim();
        var receivedFrames = 0;
        string? failure = null;

        using var stream = DriverStationRtc.StartStream(endpoint, (activeStream, result) =>
        {
            if (result != DriverStationRtcResult.Success)
            {
                failure = string.IsNullOrWhiteSpace(activeStream.Error)
                    ? $"Frame callback failed with result {result}."
                    : activeStream.Error;
                completed.Set();
                return;
            }
            if (!activeStream.TryGetNewestFrame(frame))
            {
                failure = "A successful frame callback did not expose a new frame.";
                completed.Set();
                return;
            }

            var frameNumber = Interlocked.Increment(ref receivedFrames);
            Console.WriteLine(
                $"Frame {frameNumber}: {frame.Width}x{frame.Height}, " +
                $"{frame.Length} bytes, stride {frame.Stride}, " +
                $"timestamp {frame.TimestampMicroseconds} us");
            if (frameNumber == 3)
            {
                completed.Set();
            }
        });

        if (!completed.Wait(TimeSpan.FromSeconds(20)))
        {
            Console.Error.WriteLine(
                $"Timed out after 20 seconds in state {stream.State}. " +
                $"Latest error: {stream.Error}");
            return 1;
        }
        if (failure is not null)
        {
            Console.Error.WriteLine($"Stream failed: {failure}");
            return 1;
        }

        Console.WriteLine("Successfully received three callback-driven H.264 frames.");
        return 0;
    }
    finally
    {
        DriverStationRtc.StopModule();
    }
}

static string NormalizeEndpoint(string endpointOrIp)
{
    var value = endpointOrIp.Trim();
    if (value.StartsWith("http://", StringComparison.OrdinalIgnoreCase)
        || value.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
    {
        return value;
    }

    return $"http://{value}:5807/whep";
}

static void Check(bool condition, string message)
{
    if (!condition)
    {
        throw new InvalidOperationException(message);
    }
}
