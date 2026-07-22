#ifndef DRIVER_STATION_RTC_FRAME_CONVERSION_HPP
#define DRIVER_STATION_RTC_FRAME_CONVERSION_HPP

#include "decoded_frame.hpp"

#include <cstdint>
#include <string>

namespace driverstationrtc {

bool ConvertI420ToBgra(
    const std::uint8_t* y_plane,
    const std::uint8_t* u_plane,
    const std::uint8_t* v_plane,
    int y_stride,
    int uv_stride,
    int width,
    int height,
    std::uint64_t timestamp_us,
    DecodedFrame& output,
    std::string& error);

}  // namespace driverstationrtc

#endif
