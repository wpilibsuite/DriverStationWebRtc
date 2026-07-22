#ifndef DRIVER_STATION_RTC_H264_DECODER_HPP
#define DRIVER_STATION_RTC_H264_DECODER_HPP

#include "decoded_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

class ISVCDecoder;

namespace driverstationrtc {

class H264Decoder {
public:
    H264Decoder() = default;
    ~H264Decoder();

    H264Decoder(const H264Decoder&) = delete;
    H264Decoder& operator=(const H264Decoder&) = delete;

    bool Initialize(std::string& error);
    bool Reset(std::string& error);
    bool Decode(
        const std::uint8_t* encoded,
        std::size_t length,
        std::uint64_t timestamp_us,
        DecodedFrame& output,
        bool& produced_frame,
        std::string& error);

private:
    void Destroy();

    ISVCDecoder* decoder_ = nullptr;
};

}  // namespace driverstationrtc

#endif
