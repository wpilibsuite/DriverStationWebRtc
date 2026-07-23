#include "h264_decoder.hpp"

#include "frame_conversion.hpp"

#include <wels/codec_api.h>

#include <cstring>
#include <limits>

namespace driverstationrtc {
namespace {

constexpr unsigned int kFatalDecodeStateMask =
    static_cast<unsigned int>(dsInvalidArgument) |
    static_cast<unsigned int>(dsInitialOptExpected) |
    static_cast<unsigned int>(dsOutOfMemory) |
    static_cast<unsigned int>(dsDstBufNeedExpan);

void AppendDecodeStateName(
    std::string& description,
    unsigned int state,
    DECODING_STATE flag,
    const char* name,
    bool& has_name) {
    if ((state & static_cast<unsigned int>(flag)) == 0) {
        return;
    }
    description += has_name ? ", " : " (";
    description += name;
    has_name = true;
}

std::string DescribeDecodeState(DECODING_STATE state) {
    const auto state_bits = static_cast<unsigned int>(state);
    std::string description = "OpenH264 decode state ";
    description += std::to_string(state_bits);

    bool has_name = false;
    AppendDecodeStateName(description, state_bits, dsFramePending, "frame pending", has_name);
    AppendDecodeStateName(description, state_bits, dsRefLost, "reference lost", has_name);
    AppendDecodeStateName(description, state_bits, dsBitstreamError, "bitstream error", has_name);
    AppendDecodeStateName(description, state_bits, dsDepLayerLost, "dependency layer lost", has_name);
    AppendDecodeStateName(description, state_bits, dsNoParamSets, "no parameter sets", has_name);
    AppendDecodeStateName(description, state_bits, dsDataErrorConcealed, "data error concealed", has_name);
    AppendDecodeStateName(description, state_bits, dsRefListNullPtrs, "invalid reference list", has_name);
    AppendDecodeStateName(description, state_bits, dsInvalidArgument, "invalid argument", has_name);
    AppendDecodeStateName(description, state_bits, dsInitialOptExpected, "decoder not initialized", has_name);
    AppendDecodeStateName(description, state_bits, dsOutOfMemory, "out of memory", has_name);
    AppendDecodeStateName(description, state_bits, dsDstBufNeedExpan, "destination buffer too small", has_name);
    if (has_name) {
        description += ')';
    }
    return description;
}

}  // namespace

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

H264DecodeOutcome H264Decoder::Decode(
    const std::uint8_t* encoded,
    std::size_t length,
    std::uint64_t timestamp_us,
    DecodedFrame& output,
    std::string& error) {
    if (decoder_ == nullptr || encoded == nullptr || length == 0 ||
        length > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        error = "Invalid H.264 access unit";
        return H264DecodeOutcome::FatalError;
    }

    unsigned char* planes[3] = {nullptr, nullptr, nullptr};
    SBufferInfo buffer_info{};
    buffer_info.uiInBsTimeStamp = timestamp_us;
    const DECODING_STATE state = decoder_->DecodeFrameNoDelay(
        encoded,
        static_cast<int>(length),
        planes,
        &buffer_info);

    if (state != dsErrorFree) {
        if (state == dsFramePending && buffer_info.iBufferStatus != 1) {
            error.clear();
            return H264DecodeOutcome::NeedMoreData;
        }

        error = DescribeDecodeState(state);
        const auto state_bits = static_cast<unsigned int>(state);
        return (state_bits & kFatalDecodeStateMask) == 0
                   ? H264DecodeOutcome::RecoverableError
                   : H264DecodeOutcome::FatalError;
    }

    if (buffer_info.iBufferStatus != 1) {
        error.clear();
        return H264DecodeOutcome::NeedMoreData;
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
        return H264DecodeOutcome::FatalError;
    }

    error.clear();
    return H264DecodeOutcome::FrameProduced;
}

void H264Decoder::Destroy() {
    if (decoder_ != nullptr) {
        decoder_->Uninitialize();
        WelsDestroyDecoder(decoder_);
        decoder_ = nullptr;
    }
}

}  // namespace driverstationrtc
