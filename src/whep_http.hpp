#ifndef DRIVER_STATION_RTC_WHEP_HTTP_HPP
#define DRIVER_STATION_RTC_WHEP_HTTP_HPP

#include <string>

namespace driverstationrtc {

struct WhepSessionDescription {
    std::string answer_sdp;
    std::string session_url;
};

bool StartWhepSession(
    const std::string& endpoint_url,
    const std::string& offer_sdp,
    WhepSessionDescription& session,
    std::string& error);

bool StopWhepSession(const std::string& session_url, std::string& error);

}  // namespace driverstationrtc

#endif
