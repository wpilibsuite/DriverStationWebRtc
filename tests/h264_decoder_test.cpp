#include "h264_decoder.hpp"

#include <wels/codec_api.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
            return 1;                                                           \
        }                                                                       \
    } while (false)

namespace {

std::vector<std::uint8_t> CopyAccessUnit(const SFrameBSInfo& frame_info) {
    std::vector<std::uint8_t> access_unit;
    for (int layer_index = 0; layer_index < frame_info.iLayerNum; ++layer_index) {
        const auto& layer = frame_info.sLayerInfo[layer_index];
        int layer_length = 0;
        for (int nal_index = 0; nal_index < layer.iNalCount; ++nal_index) {
            layer_length += layer.pNalLengthInByte[nal_index];
        }
        access_unit.insert(
            access_unit.end(),
            layer.pBsBuf,
            layer.pBsBuf + layer_length);
    }
    return access_unit;
}

}  // namespace

int main() {
    ISVCEncoder* encoder = nullptr;
    CHECK(WelsCreateSVCEncoder(&encoder) == 0);
    CHECK(encoder != nullptr);

    SEncParamBase encoder_parameters{};
    encoder_parameters.iUsageType = CAMERA_VIDEO_REAL_TIME;
    encoder_parameters.iPicWidth = 16;
    encoder_parameters.iPicHeight = 16;
    encoder_parameters.iTargetBitrate = 100000;
    encoder_parameters.iRCMode = RC_OFF_MODE;
    encoder_parameters.fMaxFrameRate = 30.0F;
    CHECK(encoder->Initialize(&encoder_parameters) == cmResultSuccess);

    int idr_interval = 60;
    CHECK(encoder->SetOption(ENCODER_OPTION_IDR_INTERVAL, &idr_interval) ==
          cmResultSuccess);

    std::array<std::uint8_t, 16 * 16> y_plane{};
    std::array<std::uint8_t, 8 * 8> u_plane{};
    std::array<std::uint8_t, 8 * 8> v_plane{};
    std::fill(y_plane.begin(), y_plane.end(), 96);
    std::fill(u_plane.begin(), u_plane.end(), 128);
    std::fill(v_plane.begin(), v_plane.end(), 128);

    SSourcePicture picture{};
    picture.iColorFormat = videoFormatI420;
    picture.iPicWidth = 16;
    picture.iPicHeight = 16;
    picture.iStride[0] = 16;
    picture.iStride[1] = 8;
    picture.iStride[2] = 8;
    picture.pData[0] = y_plane.data();
    picture.pData[1] = u_plane.data();
    picture.pData[2] = v_plane.data();

    SFrameBSInfo first_frame_info{};
    CHECK(encoder->EncodeFrame(&picture, &first_frame_info) == cmResultSuccess);
    CHECK(first_frame_info.eFrameType == videoFrameTypeIDR);
    const auto first_access_unit = CopyAccessUnit(first_frame_info);
    CHECK(!first_access_unit.empty());

    driverstationrtc::H264Decoder decoder;
    driverstationrtc::DecodedFrame decoded_frame;
    std::string error;
    CHECK(decoder.Initialize(error));
    CHECK(decoder.Decode(
              first_access_unit.data(),
              first_access_unit.size(),
              1000,
              decoded_frame,
              error) == driverstationrtc::H264DecodeOutcome::FrameProduced);
    CHECK(error.empty());
    CHECK(decoded_frame.width == 16);
    CHECK(decoded_frame.height == 16);

    std::fill(y_plane.begin(), y_plane.end(), 160);
    picture.uiTimeStamp = 33;
    SFrameBSInfo second_frame_info{};
    CHECK(encoder->EncodeFrame(&picture, &second_frame_info) == cmResultSuccess);
    CHECK(second_frame_info.eFrameType == videoFrameTypeP);
    const auto second_access_unit = CopyAccessUnit(second_frame_info);
    CHECK(!second_access_unit.empty());

    CHECK(decoder.Decode(
              second_access_unit.data(),
              second_access_unit.size(),
              2000,
              decoded_frame,
              error) == driverstationrtc::H264DecodeOutcome::FrameProduced);
    CHECK(error.empty());

    driverstationrtc::H264Decoder damaged_frame_decoder;
    CHECK(damaged_frame_decoder.Initialize(error));
    CHECK(damaged_frame_decoder.Decode(
              first_access_unit.data(),
              first_access_unit.size(),
              1000,
              decoded_frame,
              error) == driverstationrtc::H264DecodeOutcome::FrameProduced);
    auto damaged_access_unit = second_access_unit;
    damaged_access_unit.resize(damaged_access_unit.size() / 2);
    CHECK(damaged_frame_decoder.Decode(
              damaged_access_unit.data(),
              damaged_access_unit.size(),
              2000,
              decoded_frame,
              error) != driverstationrtc::H264DecodeOutcome::FrameProduced);

    driverstationrtc::H264Decoder decoder_without_parameter_sets;
    CHECK(decoder_without_parameter_sets.Initialize(error));
    const auto missing_parameter_sets_outcome =
        decoder_without_parameter_sets.Decode(
            second_access_unit.data(),
            second_access_unit.size(),
            2000,
            decoded_frame,
            error);
    CHECK(missing_parameter_sets_outcome ==
          driverstationrtc::H264DecodeOutcome::RecoverableError);
    CHECK(error.find("no parameter sets") != std::string::npos);

    CHECK(encoder->ForceIntraFrame(true) == cmResultSuccess);
    picture.uiTimeStamp = 66;
    SFrameBSInfo recovery_frame_info{};
    CHECK(encoder->EncodeFrame(&picture, &recovery_frame_info) == cmResultSuccess);
    CHECK(recovery_frame_info.eFrameType == videoFrameTypeIDR);
    const auto recovery_access_unit = CopyAccessUnit(recovery_frame_info);
    CHECK(decoder_without_parameter_sets.Decode(
              recovery_access_unit.data(),
              recovery_access_unit.size(),
              3000,
              decoded_frame,
              error) == driverstationrtc::H264DecodeOutcome::FrameProduced);
    CHECK(error.empty());

    CHECK(encoder->Uninitialize() == cmResultSuccess);
    WelsDestroySVCEncoder(encoder);
    return 0;
}
