#include "driver_station_rtc.h"

#include <stdio.h>

int main(void) {
    DriverStationRtcFrame frame = {0};
    DriverStationRtcStream* stream = NULL;

    if (DriverStationRtc_StartModule() != DRIVER_STATION_RTC_SUCCESS) {
        fputs("Failed to start DriverStationRtc.\n", stderr);
        return 1;
    }

    if (DriverStationRtc_StartStream("https://unsupported.invalid/whep", &stream) !=
        DRIVER_STATION_RTC_SUCCESS) {
        fputs("Failed to create a stream handle.\n", stderr);
        DriverStationRtc_StopModule();
        return 1;
    }
    if (DriverStationRtc_GetNewestFrame(stream, &frame) !=
        DRIVER_STATION_RTC_NO_FRAME) {
        fputs("A new stream unexpectedly contained a frame.\n", stderr);
        DriverStationRtc_StopStream(stream);
        DriverStationRtc_StopModule();
        return 1;
    }
    if (DriverStationRtc_SetStreamPaused(stream, 1) != DRIVER_STATION_RTC_SUCCESS ||
        DriverStationRtc_SetStreamPaused(stream, 0) != DRIVER_STATION_RTC_SUCCESS) {
        fputs("Failed to pause or resume a stream.\n", stderr);
        DriverStationRtc_StopStream(stream);
        DriverStationRtc_StopModule();
        return 1;
    }
    if (DriverStationRtc_StopStream(stream) != DRIVER_STATION_RTC_SUCCESS) {
        fputs("Failed to stop a stream.\n", stderr);
        DriverStationRtc_StopModule();
        return 1;
    }

    DriverStationRtc_FreeFrame(&frame);

    if (DriverStationRtc_StopModule() != DRIVER_STATION_RTC_SUCCESS) {
        fputs("Failed to stop DriverStationRtc.\n", stderr);
        return 1;
    }

    return 0;
}
