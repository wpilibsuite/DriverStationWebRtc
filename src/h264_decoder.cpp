#include "h264_decoder.hpp"

#include "frame_conversion.hpp"

#include <wels/codec_api.h>

#include <cstring>
#include <limits>

namespace driverstationrtc {

H264Decoder::~H264Decoder() {
    Destroy();
}

bool H264Decoder::Initialize(std::string& error) {
    Destroy();

    if (WelsCreateDecoder(&decoder_) != 0 || decoder_ == nullptr) {
        decoder_ = nullptr;
        error = "WelsCreateDecoder failed";
        return false;
    }

    SDecodingParam parameters{};
    parameters.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
    parameters.eEcActiveIdc = ERROR_CON_SLICE_COPY_CROSS_IDR_FREEZE_RES_CHANGE;
    if (decoder_->Initialize(&parameters) != 0) {
        WelsDestroyDecoder(decoder_);
        decoder_ = nullptr;
        error = "OpenH264 decoder initialization failed";
        return false;
    }

    error.clear();
    return true;
}

bool H264Decoder::Reset(std::string& error) {
    return Initialize(error);
}

bool H264Decoder::Decode(
    const std::uint8_t* encoded,
    std::size_t length,
    std::uint64_t timestamp_us,
    DecodedFrame& output,
    bool& produced_frame,
    std::string& error) {
    produced_frame = false;
    if (decoder_ == nullptr || encoded == nullptr || length == 0 ||
        length > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        error = "Invalid H.264 access unit";
        return false;
    }

    unsigned char* planes[3] = {nullptr, nullptr, nullptr};
    SBufferInfo buffer_info{};
    buffer_info.uiInBsTimeStamp = timestamp_us;
    const DECODING_STATE state = decoder_->DecodeFrameNoDelay(
        encoded,
        static_cast<int>(length),
        planes,
        &buffer_info);

    if (buffer_info.iBufferStatus != 1) {
        if (state != dsErrorFree) {
            error = "OpenH264 rejected an H.264 access unit";
            return false;
        }
        error.clear();
        return true;
    }

    const SSysMEMBuffer& system_buffer = buffer_info.UsrData.sSystemBuffer;
    if (!ConvertI420ToBgra(
            planes[0],
            planes[1],
            planes[2],
            system_buffer.iStride[0],
            system_buffer.iStride[1],
            system_buffer.iWidth,
            system_buffer.iHeight,
            timestamp_us,
            output,
            error)) {
        return false;
    }

    produced_frame = true;
    return true;
}

void H264Decoder::Destroy() {
    if (decoder_ != nullptr) {
        decoder_->Uninitialize();
        WelsDestroyDecoder(decoder_);
        decoder_ = nullptr;
    }
}

}  // namespace driverstationrtc
