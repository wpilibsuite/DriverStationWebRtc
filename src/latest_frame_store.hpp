#ifndef DRIVER_STATION_RTC_LATEST_FRAME_STORE_HPP
#define DRIVER_STATION_RTC_LATEST_FRAME_STORE_HPP

#include "decoded_frame.hpp"
#include "driver_station_rtc.h"

#include <cstdint>
#include <mutex>

namespace driverstationrtc {

class LatestFrameStore {
public:
    void Publish(DecodedFrame& frame);
    void DiscardPending();
    DriverStationRtcResult CopyNewest(DriverStationRtcFrame& destination);

private:
    std::mutex mutex_;
    DecodedFrame latest_;
    std::uint64_t sequence_ = 0;
    std::uint64_t consumed_sequence_ = 0;
};

void ReleaseFrame(DriverStationRtcFrame& frame);

}  // namespace driverstationrtc

#endif
