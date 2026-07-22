#include "whep_http.hpp"

#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

namespace driverstationrtc {
namespace {

constexpr std::size_t kMaximumResponseSize = 4U * 1024U * 1024U;
constexpr std::size_t kMaximumHeaderSize = 64U * 1024U;
constexpr std::uint32_t kReadTimeoutMs = 5000;

struct ParsedUrl {
    std::string host;
    std::string port;
    std::string authority;
    std::string target;
};

struct HttpResponse {
    int status = 0;
    std::map<std::string, std::string> headers;
    std::string body;
};

std::string Trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return std::string(value);
}

std::string Lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool ParseUrl(const std::string& input, ParsedUrl& output, std::string& error) {
    std::string url = input;
    if (url.find("://") == std::string::npos) {
        url.insert(0, "http://");
    }
    if (url.rfind("http://", 0) != 0) {
        error = "Only plain HTTP WHEP endpoints are currently supported";
        return false;
    }

    const std::size_t authority_begin = 7;
    const std::size_t authority_end = url.find_first_of("/?#", authority_begin);
    output.authority = url.substr(
        authority_begin,
        authority_end == std::string::npos ? std::string::npos
                                           : authority_end - authority_begin);
    if (output.authority.empty() || output.authority.find('@') != std::string::npos) {
        error = "The WHEP URL has an invalid authority";
        return false;
    }

    if (authority_end == std::string::npos) {
        output.target = "/";
    } else {
        output.target = url.substr(authority_end);
        const std::size_t fragment = output.target.find('#');
        if (fragment != std::string::npos) {
            output.target.erase(fragment);
        }
        if (output.target.empty() || output.target.front() == '?') {
            output.target.insert(output.target.begin(), '/');
        }
    }

    output.port = "80";
    if (output.authority.front() == '[') {
        const std::size_t close_bracket = output.authority.find(']');
        if (close_bracket == std::string::npos) {
            error = "The WHEP URL has an invalid IPv6 address";
            return false;
        }
        output.host = output.authority.substr(1, close_bracket - 1);
        if (close_bracket + 1 < output.authority.size()) {
            if (output.authority[close_bracket + 1] != ':') {
                error = "The WHEP URL has an invalid IPv6 port";
                return false;
            }
            output.port = output.authority.substr(close_bracket + 2);
        }
    } else {
        const std::size_t colon = output.authority.rfind(':');
        if (colon != std::string::npos) {
            output.host = output.authority.substr(0, colon);
            output.port = output.authority.substr(colon + 1);
        } else {
            output.host = output.authority;
        }
    }

    if (output.host.empty() || output.port.empty() ||
        !std::all_of(output.port.begin(), output.port.end(), [](unsigned char character) {
            return std::isdigit(character) != 0;
        })) {
        error = "The WHEP URL has an invalid host or port";
        return false;
    }

    unsigned int port_number = 0;
    const auto port_result = std::from_chars(
        output.port.data(),
        output.port.data() + output.port.size(),
        port_number);
    if (port_result.ec != std::errc{} || port_number == 0 || port_number > 65535) {
        error = "The WHEP URL port is outside the valid range";
        return false;
    }

    error.clear();
    return true;
}

std::string MbedTlsError(int result) {
    char buffer[256]{};
    mbedtls_strerror(result, buffer, sizeof(buffer));
    return buffer;
}

bool SendAll(mbedtls_net_context& socket, const std::string& request, std::string& error) {
    std::size_t offset = 0;
    while (offset < request.size()) {
        const int sent = mbedtls_net_send(
            &socket,
            reinterpret_cast<const unsigned char*>(request.data() + offset),
            request.size() - offset);
        if (sent <= 0) {
            error = "Failed to send WHEP HTTP request: " + MbedTlsError(sent);
            return false;
        }
        offset += static_cast<std::size_t>(sent);
    }
    return true;
}

bool DecodeChunkedBody(const std::string& encoded, std::string& decoded, std::string& error) {
    std::size_t cursor = 0;
    decoded.clear();
    while (true) {
        const std::size_t line_end = encoded.find("\r\n", cursor);
        if (line_end == std::string::npos) {
            error = "The WHEP HTTP response has an incomplete chunk header";
            return false;
        }

        std::string_view size_text(encoded.data() + cursor, line_end - cursor);
        const std::size_t extension = size_text.find(';');
        if (extension != std::string_view::npos) {
            size_text = size_text.substr(0, extension);
        }
        std::size_t chunk_size = 0;
        const auto result = std::from_chars(
            size_text.data(),
            size_text.data() + size_text.size(),
            chunk_size,
            16);
        if (result.ec != std::errc{}) {
            error = "The WHEP HTTP response has an invalid chunk size";
            return false;
        }

        cursor = line_end + 2;
        if (chunk_size == 0) {
            return true;
        }
        if (chunk_size > encoded.size() - cursor ||
            encoded.size() - cursor - chunk_size < 2 ||
            encoded.compare(cursor + chunk_size, 2, "\r\n") != 0) {
            error = "The WHEP HTTP response has an incomplete chunk";
            return false;
        }
        if (decoded.size() > kMaximumResponseSize - chunk_size) {
            error = "The WHEP HTTP response body is too large";
            return false;
        }
        decoded.append(encoded, cursor, chunk_size);
        cursor += chunk_size + 2;
    }
}

bool ParseResponse(const std::string& bytes, HttpResponse& response, std::string& error) {
    const std::size_t header_end = bytes.find("\r\n\r\n");
    if (header_end == std::string::npos || header_end > kMaximumHeaderSize) {
        error = "The WHEP server returned an invalid HTTP header";
        return false;
    }

    const std::size_t status_end = bytes.find("\r\n");
    if (status_end == std::string::npos) {
        error = "The WHEP server returned an invalid HTTP status line";
        return false;
    }
    std::istringstream status_stream(bytes.substr(0, status_end));
    std::string http_version;
    status_stream >> http_version >> response.status;
    if (http_version.rfind("HTTP/", 0) != 0 || response.status < 100 || response.status > 599) {
        error = "The WHEP server returned an invalid HTTP status";
        return false;
    }

    std::size_t line_begin = status_end + 2;
    while (line_begin < header_end) {
        const std::size_t line_end = bytes.find("\r\n", line_begin);
        if (line_end == std::string::npos || line_end > header_end) {
            error = "The WHEP server returned an invalid HTTP header line";
            return false;
        }
        const std::size_t colon = bytes.find(':', line_begin);
        if (colon == std::string::npos || colon >= line_end) {
            error = "The WHEP server returned an invalid HTTP header field";
            return false;
        }
        response.headers[Lowercase(Trim(std::string_view(
            bytes.data() + line_begin,
            colon - line_begin)))] = Trim(std::string_view(
                bytes.data() + colon + 1,
                line_end - colon - 1));
        line_begin = line_end + 2;
    }

    std::string encoded_body = bytes.substr(header_end + 4);
    const auto transfer_encoding = response.headers.find("transfer-encoding");
    if (transfer_encoding != response.headers.end() &&
        Lowercase(transfer_encoding->second).find("chunked") != std::string::npos) {
        return DecodeChunkedBody(encoded_body, response.body, error);
    }

    const auto content_length = response.headers.find("content-length");
    if (content_length != response.headers.end()) {
        std::size_t expected_length = 0;
        const auto length_result = std::from_chars(
            content_length->second.data(),
            content_length->second.data() + content_length->second.size(),
            expected_length);
        if (length_result.ec != std::errc{} || expected_length > encoded_body.size()) {
            error = "The WHEP server returned an incomplete HTTP body";
            return false;
        }
        encoded_body.resize(expected_length);
    }
    response.body = std::move(encoded_body);
    return true;
}

bool ResponseIsComplete(const std::string& bytes) {
    const std::size_t header_end = bytes.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return false;
    }

    const std::string headers = Lowercase(bytes.substr(0, header_end));
    const std::size_t body_begin = header_end + 4;
    const std::size_t content_length_header = headers.find("\r\ncontent-length:");
    if (content_length_header != std::string::npos) {
        const std::size_t value_begin =
            content_length_header + std::string_view("\r\ncontent-length:").size();
        const std::size_t value_end = headers.find("\r\n", value_begin);
        const std::string value = Trim(std::string_view(
            headers.data() + value_begin,
            (value_end == std::string::npos ? header_end : value_end) - value_begin));
        std::size_t expected_length = 0;
        const auto result = std::from_chars(
            value.data(),
            value.data() + value.size(),
            expected_length);
        return result.ec == std::errc{} && body_begin <= bytes.size() &&
               expected_length <= bytes.size() - body_begin;
    }

    if (headers.find("\r\ntransfer-encoding:") != std::string::npos &&
        headers.find("chunked") != std::string::npos) {
        const std::string_view body(bytes.data() + body_begin, bytes.size() - body_begin);
        return body == "0\r\n\r\n" ||
               body.find("\r\n0\r\n\r\n") != std::string_view::npos;
    }
    return false;
}

bool PerformRequest(
    const ParsedUrl& url,
    std::string_view method,
    std::string_view content_type,
    const std::string& body,
    HttpResponse& response,
    std::string& error) {
    mbedtls_net_context socket;
    mbedtls_net_init(&socket);
    const int connect_result = mbedtls_net_connect(
        &socket,
        url.host.c_str(),
        url.port.c_str(),
        MBEDTLS_NET_PROTO_TCP);
    if (connect_result != 0) {
        error = "Failed to connect to the WHEP endpoint: " + MbedTlsError(connect_result);
        mbedtls_net_free(&socket);
        return false;
    }

    std::ostringstream request;
    request << method << ' ' << url.target << " HTTP/1.1\r\n"
            << "Host: " << url.authority << "\r\n"
            << "Accept: application/sdp\r\n"
            << "Connection: close\r\n";
    if (!content_type.empty()) {
        request << "Content-Type: " << content_type << "\r\n";
    }
    request << "Content-Length: " << body.size() << "\r\n\r\n" << body;

    const std::string serialized_request = request.str();
    if (!SendAll(socket, serialized_request, error)) {
        mbedtls_net_free(&socket);
        return false;
    }

    std::string response_bytes;
    unsigned char buffer[4096];
    while (response_bytes.size() <= kMaximumResponseSize) {
        const int received = mbedtls_net_recv_timeout(
            &socket,
            buffer,
            sizeof(buffer),
            kReadTimeoutMs);
        if (received == 0 || received == MBEDTLS_ERR_NET_CONN_RESET) {
            break;
        }
        if (received < 0) {
            error = "Failed to receive the WHEP HTTP response: " + MbedTlsError(received);
            mbedtls_net_free(&socket);
            return false;
        }
        response_bytes.append(
            reinterpret_cast<const char*>(buffer),
            static_cast<std::size_t>(received));
        if (ResponseIsComplete(response_bytes)) {
            break;
        }
    }
    mbedtls_net_free(&socket);

    if (response_bytes.size() > kMaximumResponseSize) {
        error = "The WHEP HTTP response is too large";
        return false;
    }
    return ParseResponse(response_bytes, response, error);
}

bool ResolveLocation(
    const ParsedUrl& endpoint,
    const std::string& location,
    std::string& session_url,
    std::string& error) {
    if (location.empty()) {
        error = "The WHEP server did not return a session Location header";
        return false;
    }
    if (location.rfind("http://", 0) == 0) {
        session_url = location;
        return true;
    }
    if (location.rfind("https://", 0) == 0) {
        error = "The WHEP server returned an HTTPS session URL, which is not supported yet";
        return false;
    }

    session_url = "http://" + endpoint.authority;
    if (location.front() == '/') {
        session_url += location;
        return true;
    }

    const std::size_t query = endpoint.target.find('?');
    std::string base = endpoint.target.substr(0, query);
    const std::size_t slash = base.rfind('/');
    base.resize(slash == std::string::npos ? 1 : slash + 1);
    session_url += base + location;
    return true;
}

}  // namespace

bool StartWhepSession(
    const std::string& endpoint_url,
    const std::string& offer_sdp,
    WhepSessionDescription& session,
    std::string& error) {
    ParsedUrl endpoint;
    if (!ParseUrl(endpoint_url, endpoint, error)) {
        return false;
    }

    HttpResponse response;
    if (!PerformRequest(
            endpoint,
            "POST",
            "application/sdp",
            offer_sdp,
            response,
            error)) {
        return false;
    }
    if (response.status != 200 && response.status != 201) {
        error = "The WHEP endpoint rejected the offer with HTTP " +
                std::to_string(response.status);
        return false;
    }
    if (response.body.empty()) {
        error = "The WHEP endpoint returned an empty SDP answer";
        return false;
    }

    const auto location = response.headers.find("location");
    session.session_url.clear();
    // Some embedded WHEP implementations return a usable SDP answer without
    // creating an HTTP session resource. Streaming still works; only the
    // optional DELETE cleanup request is unavailable in that case.
    if (location != response.headers.end() &&
        !ResolveLocation(endpoint, location->second, session.session_url, error)) {
        return false;
    }
    session.answer_sdp = std::move(response.body);
    error.clear();
    return true;
}

bool StopWhepSession(const std::string& session_url, std::string& error) {
    if (session_url.empty()) {
        error.clear();
        return true;
    }

    ParsedUrl endpoint;
    if (!ParseUrl(session_url, endpoint, error)) {
        return false;
    }
    HttpResponse response;
    if (!PerformRequest(endpoint, "DELETE", "", "", response, error)) {
        return false;
    }
    if (response.status != 200 && response.status != 202 &&
        response.status != 204 && response.status != 404) {
        error = "The WHEP endpoint rejected session shutdown with HTTP " +
                std::to_string(response.status);
        return false;
    }
    error.clear();
    return true;
}

}  // namespace driverstationrtc
