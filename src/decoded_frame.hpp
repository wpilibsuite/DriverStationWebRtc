#ifndef DRIVER_STATION_RTC_DECODED_FRAME_HPP
#define DRIVER_STATION_RTC_DECODED_FRAME_HPP

#include <cstdint>
#include <vector>

namespace driverstationrtc {

struct DecodedFrame {
    std::vector<std::uint8_t> pixels;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride = 0;
    std::uint64_t timestamp_us = 0;
};

}  // namespace driverstationrtc

#endif
