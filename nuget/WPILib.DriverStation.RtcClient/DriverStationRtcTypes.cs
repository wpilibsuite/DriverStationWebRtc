namespace WPILib.DriverStation.RtcClient;

/// <summary>Result codes returned by the native DriverStationRtc library.</summary>
public enum DriverStationRtcResult
{
    /// <summary>The operation succeeded.</summary>
    Success = 0,
    /// <summary>No new decoded frame is available.</summary>
    NoFrame = 1,
    /// <summary>An argument or handle is invalid.</summary>
    InvalidArgument = -1,
    /// <summary>The process-wide module has not been started.</summary>
    ModuleNotStarted = -2,
    /// <summary>A native allocation failed.</summary>
    OutOfMemory = -3,
    /// <summary>The requested operation is invalid in the current state.</summary>
    InvalidState = -4,
    /// <summary>A WHEP network operation failed.</summary>
    NetworkError = -5,
    /// <summary>The H.264 decoder could not complete the operation.</summary>
    DecodeError = -6,
    /// <summary>An unexpected native error occurred.</summary>
    InternalError = -7,
}

/// <summary>The current state of an asynchronous WHEP stream.</summary>
public enum DriverStationRtcStreamState
{
    /// <summary>The WHEP and WebRTC connection is being established.</summary>
    Connecting = 0,
    /// <summary>The stream is connected and decoding frames.</summary>
    Running = 1,
    /// <summary>The stream has been stopped.</summary>
    Stopped = 2,
    /// <summary>The asynchronous stream setup or transport failed.</summary>
    Failed = 3,
}

/// <summary>The decoded frame's memory layout.</summary>
public enum DriverStationRtcPixelFormat
{
    /// <summary>Four bytes per pixel in blue, green, red, opaque-alpha order.</summary>
    Bgra8888 = 1,
}
