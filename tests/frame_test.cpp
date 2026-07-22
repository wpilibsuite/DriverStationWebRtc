#include "frame_conversion.hpp"
#include "latest_frame_store.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
            return 1;                                                           \
        }                                                                       \
    } while (false)

int main() {
    const std::uint8_t y_plane[] = {16, 235, 16, 235};
    const std::uint8_t u_plane[] = {128};
    const std::uint8_t v_plane[] = {128};
    driverstationrtc::DecodedFrame converted;
    std::string error;
    CHECK(driverstationrtc::ConvertI420ToBgra(
        y_plane,
        u_plane,
        v_plane,
        2,
        1,
        2,
        2,
        1234,
        converted,
        error));
    CHECK(converted.width == 2);
    CHECK(converted.height == 2);
    CHECK(converted.stride == 8);
    CHECK(converted.timestamp_us == 1234);
    CHECK(converted.pixels.size() == 16);
    CHECK(converted.pixels[0] == 0);
    CHECK(converted.pixels[1] == 0);
    CHECK(converted.pixels[2] == 0);
    CHECK(converted.pixels[3] == 255);
    CHECK(converted.pixels[4] == 255);
    CHECK(converted.pixels[5] == 255);
    CHECK(converted.pixels[6] == 255);
    CHECK(converted.pixels[7] == 255);

    driverstationrtc::LatestFrameStore store;
    DriverStationRtcFrame destination{};
    CHECK(store.CopyNewest(destination) == DRIVER_STATION_RTC_NO_FRAME);
    CHECK(destination.data == nullptr);

    store.Publish(converted);
    CHECK(store.CopyNewest(destination) == DRIVER_STATION_RTC_SUCCESS);
    CHECK(destination.data != nullptr);
    CHECK(destination.length == 16);
    CHECK(destination.capacity >= 16);
    CHECK(destination.width == 2);
    CHECK(destination.height == 2);
    CHECK(destination.stride == 8);
    CHECK(destination.pixel_format == DRIVER_STATION_RTC_PIXEL_FORMAT_BGRA8888);
    CHECK(destination.timestamp_us == 1234);

    std::uint8_t* reused_pointer = destination.data;
    CHECK(store.CopyNewest(destination) == DRIVER_STATION_RTC_NO_FRAME);
    CHECK(destination.data == reused_pointer);
    CHECK(destination.length == 0);

    driverstationrtc::DecodedFrame older;
    older.pixels.assign(4, 1);
    older.width = 1;
    older.height = 1;
    older.stride = 4;
    older.timestamp_us = 2000;
    store.Publish(older);

    driverstationrtc::DecodedFrame newest;
    newest.pixels.assign(64, 2);
    newest.width = 4;
    newest.height = 4;
    newest.stride = 16;
    newest.timestamp_us = 3000;
    store.Publish(newest);

    CHECK(store.CopyNewest(destination) == DRIVER_STATION_RTC_SUCCESS);
    CHECK(destination.length == 64);
    CHECK(destination.width == 4);
    CHECK(destination.height == 4);
    CHECK(destination.timestamp_us == 3000);
    CHECK(destination.data[0] == 2);

    driverstationrtc::ReleaseFrame(destination);
    CHECK(destination.data == nullptr);
    CHECK(destination.capacity == 0);
    return 0;
}
