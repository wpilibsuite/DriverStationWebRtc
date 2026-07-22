#ifndef DRIVER_STATION_RTC_H
#define DRIVER_STATION_RTC_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(DRIVER_STATION_RTC_BUILDING_LIBRARY)
#    define DRIVER_STATION_RTC_API __declspec(dllexport)
#  else
#    define DRIVER_STATION_RTC_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define DRIVER_STATION_RTC_API __attribute__((visibility("default")))
#else
#  define DRIVER_STATION_RTC_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque stream handle. Store this as an IntPtr or SafeHandle in .NET. */
typedef struct DriverStationRtcStream DriverStationRtcStream;

typedef enum DriverStationRtcResult {
    DRIVER_STATION_RTC_SUCCESS = 0,
    DRIVER_STATION_RTC_NO_FRAME = 1,
    DRIVER_STATION_RTC_INVALID_ARGUMENT = -1,
    DRIVER_STATION_RTC_MODULE_NOT_STARTED = -2,
    DRIVER_STATION_RTC_OUT_OF_MEMORY = -3,
    DRIVER_STATION_RTC_INVALID_STATE = -4,
    DRIVER_STATION_RTC_NETWORK_ERROR = -5,
    DRIVER_STATION_RTC_DECODE_ERROR = -6,
    DRIVER_STATION_RTC_INTERNAL_ERROR = -7
} DriverStationRtcResult;

typedef enum DriverStationRtcStreamState {
    DRIVER_STATION_RTC_STREAM_CONNECTING = 0,
    DRIVER_STATION_RTC_STREAM_RUNNING = 1,
    DRIVER_STATION_RTC_STREAM_PAUSED = 2,
    DRIVER_STATION_RTC_STREAM_STOPPED = 3,
    DRIVER_STATION_RTC_STREAM_FAILED = 4
} DriverStationRtcStreamState;

typedef enum DriverStationRtcPixelFormat {
    /** Four bytes per pixel in memory: blue, green, red, then opaque alpha. */
    DRIVER_STATION_RTC_PIXEL_FORMAT_BGRA8888 = 1
} DriverStationRtcPixelFormat;

/**
 * An Avalonia-ready decoded bitmap and its library-owned memory buffer.
 *
 * Initialize the whole structure to zero before its first use. The library
 * allocates or grows data as necessary. Pass the same structure back to
 * DriverStationRtc_GetNewestFrame() to reuse its allocation, and call
 * DriverStationRtc_FreeFrame() once when the caller no longer needs it.
 * A non-null data pointer must have been allocated by this library.
 */
typedef struct DriverStationRtcFrame {
    uint8_t* data;
    size_t length;
    size_t capacity;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    DriverStationRtcPixelFormat pixel_format;
    uint64_t timestamp_us;
} DriverStationRtcFrame;

/** Starts process-wide WebRTC resources. Idempotent. */
DRIVER_STATION_RTC_API DriverStationRtcResult DriverStationRtc_StartModule(void);

/**
 * Stops every active stream and releases process-wide WebRTC resources.
 * All stream handles become invalid. Idempotent.
 */
DRIVER_STATION_RTC_API DriverStationRtcResult DriverStationRtc_StopModule(void);

/**
 * Starts an asynchronous WHEP receive session.
 *
 * url may be either an HTTP URL or host:port/path; for example,
 * "http://limelight.local:5807/whep". The returned handle is valid while the
 * connection is still being established. Poll DriverStationRtc_GetStreamState
 * to distinguish connecting, running, paused, and failed streams.
 */
DRIVER_STATION_RTC_API DriverStationRtcResult DriverStationRtc_StartStream(
    const char* url,
    DriverStationRtcStream** stream);

/**
 * Pauses or resumes decode and frame delivery without closing the WHEP session.
 * Pass nonzero to pause and zero to resume. Resume requests a fresh keyframe.
 */
DRIVER_STATION_RTC_API DriverStationRtcResult DriverStationRtc_SetStreamPaused(
    DriverStationRtcStream* stream,
    int paused);

/**
 * Requests one new decoded frame while the stream remains paused.
 *
 * The decoder is reset, an H.264 keyframe is requested, and encoded frames are
 * consumed only until one bitmap is published. The bitmap is then available
 * through DriverStationRtc_GetNewestFrame(). This operation is valid only for
 * a paused stream. Repeated requests are coalesced while one is pending.
 */
DRIVER_STATION_RTC_API DriverStationRtcResult DriverStationRtc_RequestFrame(
    DriverStationRtcStream* stream);

/** Stops a stream, releases its resources, and invalidates its handle. */
DRIVER_STATION_RTC_API DriverStationRtcResult DriverStationRtc_StopStream(
    DriverStationRtcStream* stream);

/** Returns the current asynchronous connection state. */
DRIVER_STATION_RTC_API DriverStationRtcStreamState DriverStationRtc_GetStreamState(
    DriverStationRtcStream* stream);

/**
 * Returns the stream's latest asynchronous error as a thread-local UTF-8
 * string. The pointer remains valid until the next call on the same thread.
 */
DRIVER_STATION_RTC_API const char* DriverStationRtc_GetStreamError(
    DriverStationRtcStream* stream);

/**
 * Copies the newest decoded BGRA8888 bitmap into frame.
 *
 * Returns DRIVER_STATION_RTC_NO_FRAME when no frame has arrived since the last
 * successful call for this stream. In that case length and bitmap metadata are
 * cleared, while data and capacity are retained for reuse.
 */
DRIVER_STATION_RTC_API DriverStationRtcResult DriverStationRtc_GetNewestFrame(
    DriverStationRtcStream* stream,
    DriverStationRtcFrame* frame);

/** Frees a frame buffer allocated by this library and zeros the structure. */
DRIVER_STATION_RTC_API void DriverStationRtc_FreeFrame(DriverStationRtcFrame* frame);

#ifdef __cplusplus
}
#endif

#endif
