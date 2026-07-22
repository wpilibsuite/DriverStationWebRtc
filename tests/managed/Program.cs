using WPILib.DriverStation.RtcClient;

return args.Length == 0 ? RunOfflineSmoke() : RunLiveStream(args[0]);

static int RunOfflineSmoke()
{
    DriverStationRtc.StartModule();
    try
    {
        using var stream = DriverStationRtc.StartStream("https://unsupported.invalid/whep");
        using var frame = new DriverStationRtcFrame();

        Check(!stream.TryGetNewestFrame(frame), "A new stream unexpectedly returned a frame.");
        Check(!frame.HasData, "The empty reusable frame buffer reports pixel data.");
        Check(frame.Pixels.IsEmpty, "The empty reusable frame buffer exposes pixel bytes.");

        bool runningRequestRejected = false;
        try
        {
            stream.RequestFrame();
        }
        catch (DriverStationRtcException exception)
            when (exception.Result == DriverStationRtcResult.InvalidState)
        {
            runningRequestRejected = true;
        }

        Check(runningRequestRejected, "A running stream accepted a paused-frame request.");

        stream.Pause();
        stream.RequestFrame();
        stream.Resume();
        stream.Stop();

        bool disposedStreamRejected = false;
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
    string endpoint = NormalizeEndpoint(endpointOrIp);
    Console.WriteLine($"Connecting to {endpoint}");

    DriverStationRtc.StartModule();
    try
    {
        using var stream = DriverStationRtc.StartStream(endpoint);
        using var frame = new DriverStationRtcFrame();
        DriverStationRtcStreamState? previousState = null;
        DateTime deadline = DateTime.UtcNow.AddSeconds(20);
        int receivedFrames = 0;

        while (DateTime.UtcNow < deadline)
        {
            DriverStationRtcStreamState state = stream.State;
            if (state != previousState)
            {
                Console.WriteLine($"Stream state: {state}");
                previousState = state;
            }

            if (state == DriverStationRtcStreamState.Failed)
            {
                Console.Error.WriteLine($"Stream failed: {stream.Error}");
                return 1;
            }

            if (stream.TryGetNewestFrame(frame))
            {
                ++receivedFrames;
                Console.WriteLine(
                    $"Frame {receivedFrames}: {frame.Width}x{frame.Height}, " +
                    $"{frame.Length} bytes, stride {frame.Stride}, " +
                    $"timestamp {frame.TimestampMicroseconds} us");
                if (receivedFrames == 3)
                {
                    Console.WriteLine("Successfully received and decoded the H.264 stream.");
                    return 0;
                }
            }

            Thread.Sleep(10);
        }

        Console.Error.WriteLine(
            $"Timed out after 20 seconds in state {stream.State}. " +
            $"Latest error: {stream.Error}");
        return 1;
    }
    finally
    {
        DriverStationRtc.StopModule();
    }
}

static string NormalizeEndpoint(string endpointOrIp)
{
    string value = endpointOrIp.Trim();
    if (value.StartsWith("http://", StringComparison.OrdinalIgnoreCase) ||
        value.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
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
