#include "driver_station_rtc.h"

#include <stdio.h>

static void on_frame(DriverStationRtcResult result, void* user_data) {
    (void)result;
    int* callback_count = (int*)user_data;
    ++(*callback_count);
}

int main(void) {
    DriverStationRtcFrame frame = {0};
    DriverStationRtcStream* stream = NULL;
    int callback_count = 0;

    if (DriverStationRtc_StartModule() != DRIVER_STATION_RTC_SUCCESS) {
        fputs("Failed to start DriverStationRtc.\n", stderr);
        return 1;
    }

    if (DriverStationRtc_StartStream(
            "https://unsupported.invalid/whep",
            on_frame,
            &callback_count,
            &stream) !=
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
    if (DriverStationRtc_StopStream(stream) != DRIVER_STATION_RTC_SUCCESS) {
        fputs("Failed to stop a stream.\n", stderr);
        DriverStationRtc_StopModule();
        return 1;
    }
    stream = NULL;
    int callback_count_after_stop = callback_count;

    DriverStationRtc_FreeFrame(&frame);

    if (DriverStationRtc_StopModule() != DRIVER_STATION_RTC_SUCCESS) {
        fputs("Failed to stop DriverStationRtc.\n", stderr);
        return 1;
    }
    if (callback_count != callback_count_after_stop) {
        fputs("A frame callback ran after its stream was stopped.\n", stderr);
        return 1;
    }

    return 0;
}
