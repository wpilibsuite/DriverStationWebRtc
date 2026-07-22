#include "frame_conversion.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace driverstationrtc {
namespace {

std::uint8_t ClampToByte(int value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

}  // namespace

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
    std::string& error) {
    const int chroma_width = width / 2 + width % 2;
    if (y_plane == nullptr || u_plane == nullptr || v_plane == nullptr ||
        width <= 0 || height <= 0 || y_stride < width ||
        uv_stride < chroma_width) {
        error = "OpenH264 returned invalid I420 plane metadata";
        return false;
    }

    const auto unsigned_width = static_cast<std::size_t>(width);
    const auto unsigned_height = static_cast<std::size_t>(height);
    if (unsigned_width > std::numeric_limits<std::uint32_t>::max() / 4U ||
        unsigned_height > std::numeric_limits<std::size_t>::max() /
                              (unsigned_width * 4U)) {
        error = "Decoded frame dimensions overflow the BGRA buffer size";
        return false;
    }

    const std::size_t output_stride = unsigned_width * 4U;
    const std::size_t output_length = output_stride * unsigned_height;
    output.pixels.resize(output_length);

    // OpenH264 outputs limited-range I420. Convert with the integer BT.601
    // matrix and write Avalonia's native Bgra8888 byte order.
    for (int row = 0; row < height; ++row) {
        const std::uint8_t* y_row = y_plane + static_cast<std::size_t>(row) * y_stride;
        const std::uint8_t* u_row = u_plane + static_cast<std::size_t>(row / 2) * uv_stride;
        const std::uint8_t* v_row = v_plane + static_cast<std::size_t>(row / 2) * uv_stride;
        std::uint8_t* destination =
            output.pixels.data() + static_cast<std::size_t>(row) * output_stride;

        for (int column = 0; column < width; ++column) {
            const int y = std::max(0, static_cast<int>(y_row[column]) - 16);
            const int u = static_cast<int>(u_row[column / 2]) - 128;
            const int v = static_cast<int>(v_row[column / 2]) - 128;

            const int red = (298 * y + 409 * v + 128) >> 8;
            const int green = (298 * y - 100 * u - 208 * v + 128) >> 8;
            const int blue = (298 * y + 516 * u + 128) >> 8;

            destination[0] = ClampToByte(blue);
            destination[1] = ClampToByte(green);
            destination[2] = ClampToByte(red);
            destination[3] = 255;
            destination += 4;
        }
    }

    output.width = static_cast<std::uint32_t>(width);
    output.height = static_cast<std::uint32_t>(height);
    output.stride = static_cast<std::uint32_t>(output_stride);
    output.timestamp_us = timestamp_us;
    error.clear();
    return true;
}

}  // namespace driverstationrtc
