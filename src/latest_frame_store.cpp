#include "latest_frame_store.hpp"

#include <cstdlib>
#include <cstring>

namespace driverstationrtc {

void LatestFrameStore::Publish(DecodedFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::swap(latest_, frame);
    ++sequence_;
}

DriverStationRtcResult LatestFrameStore::CopyNewest(DriverStationRtcFrame& destination) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sequence_ == consumed_sequence_) {
        destination.length = 0;
        destination.width = 0;
        destination.height = 0;
        destination.stride = 0;
        destination.pixel_format = DRIVER_STATION_RTC_PIXEL_FORMAT_BGRA8888;
        destination.timestamp_us = 0;
        return DRIVER_STATION_RTC_NO_FRAME;
    }

    if (destination.data == nullptr && destination.capacity != 0) {
        return DRIVER_STATION_RTC_INVALID_ARGUMENT;
    }

    const std::size_t required = latest_.pixels.size();
    if (destination.capacity < required) {
        void* replacement = std::realloc(destination.data, required);
        if (replacement == nullptr && required != 0) {
            return DRIVER_STATION_RTC_OUT_OF_MEMORY;
        }
        destination.data = static_cast<std::uint8_t*>(replacement);
        destination.capacity = required;
    }

    if (required != 0) {
        std::memcpy(destination.data, latest_.pixels.data(), required);
    }
    destination.length = required;
    destination.width = latest_.width;
    destination.height = latest_.height;
    destination.stride = latest_.stride;
    destination.pixel_format = DRIVER_STATION_RTC_PIXEL_FORMAT_BGRA8888;
    destination.timestamp_us = latest_.timestamp_us;
    consumed_sequence_ = sequence_;
    return DRIVER_STATION_RTC_SUCCESS;
}

void ReleaseFrame(DriverStationRtcFrame& frame) {
    std::free(frame.data);
    frame = {};
}

}  // namespace driverstationrtc
