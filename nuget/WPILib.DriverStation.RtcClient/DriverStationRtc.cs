namespace WPILib.DriverStation.RtcClient;

/// <summary>Controls the process-wide DriverStationRtc module.</summary>
public static class DriverStationRtc
{
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
    /// <returns>A disposable stream whose state can be polled while it connects.</returns>
    public static DriverStationRtcStream StartStream(string url)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(url);

        DriverStationRtcResult result = NativeMethods.StartStream(url, out nint stream);
        if (result != DriverStationRtcResult.Success)
        {
            if (stream != 0)
            {
                NativeMethods.StopStream(stream);
            }

            ThrowIfFailed(result);
        }
        if (stream == 0)
        {
            throw new DriverStationRtcException(
                DriverStationRtcResult.InternalError,
                "The native library returned a null stream handle.");
        }

        return new DriverStationRtcStream(new SafeStreamHandle(stream));
    }

    internal static void ThrowIfFailed(DriverStationRtcResult result, string? detail = null)
    {
        if (result != DriverStationRtcResult.Success)
        {
            throw new DriverStationRtcException(result, detail);
        }
    }
}
