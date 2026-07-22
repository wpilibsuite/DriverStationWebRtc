namespace WPILib.DriverStation.RtcClient;

/// <summary>An error reported by the native DriverStationRtc library.</summary>
public sealed class DriverStationRtcException : Exception
{
    internal DriverStationRtcException(DriverStationRtcResult result, string? detail = null)
        : base(CreateMessage(result, detail))
    {
        Result = result;
    }

    /// <summary>Gets the native result code that caused the exception.</summary>
    public DriverStationRtcResult Result { get; }

    private static string CreateMessage(DriverStationRtcResult result, string? detail)
    {
        string prefix = result switch
        {
            DriverStationRtcResult.InvalidArgument => "DriverStationRtc received an invalid argument.",
            DriverStationRtcResult.ModuleNotStarted => "The DriverStationRtc module has not been started.",
            DriverStationRtcResult.OutOfMemory => "DriverStationRtc could not allocate a frame buffer.",
            DriverStationRtcResult.InvalidState => "DriverStationRtc is not in a valid state for this operation.",
            DriverStationRtcResult.NetworkError => "The WHEP network operation failed.",
            DriverStationRtcResult.DecodeError => "OpenH264 could not decode the stream.",
            _ => "DriverStationRtc reported an internal error.",
        };

        return string.IsNullOrWhiteSpace(detail) ? prefix : $"{prefix} {detail}";
    }
}
