using WPILib.DriverStation.RtcClient;

static void Check(bool condition, string message)
{
    if (!condition)
    {
        throw new InvalidOperationException(message);
    }
}

DriverStationRtc.StartModule();
try
{
    using var stream = DriverStationRtc.StartStream("https://unsupported.invalid/whep");
    using var frame = new DriverStationRtcFrame();

    Check(!stream.TryGetNewestFrame(frame), "A new stream unexpectedly returned a frame.");
    Check(!frame.HasData, "The empty reusable frame buffer reports pixel data.");
    Check(frame.Pixels.IsEmpty, "The empty reusable frame buffer exposes pixel bytes.");

    stream.Pause();
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
}
finally
{
    DriverStationRtc.StopModule();
}
