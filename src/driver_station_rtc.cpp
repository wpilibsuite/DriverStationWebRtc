#include "driver_station_rtc.h"

#include "h264_decoder.hpp"
#include "latest_frame_store.hpp"
#include "whep_http.hpp"

#include <rtc/h264rtpdepacketizer.hpp>
#include <rtc/rtc.hpp>
#include <rtc/rtcpreceivingsession.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

struct DriverStationRtcStream {
    std::uint64_t reserved = 0;
};

namespace driverstationrtc {
namespace {

constexpr auto kIceGatheringTimeout = std::chrono::seconds(10);

class StreamImpl : public std::enable_shared_from_this<StreamImpl> {
public:
    explicit StreamImpl(std::string url) : url_(std::move(url)) {}

    ~StreamImpl() {
        Stop();
    }

    bool Start(std::string& error) {
        std::lock_guard<std::mutex> decoder_lock(decoder_mutex_);
        if (!decoder_.Initialize(error)) {
            state_.store(DRIVER_STATION_RTC_STREAM_FAILED);
            SetError(error);
            return false;
        }
        worker_ = std::thread([self = shared_from_this()] { self->Run(); });
        return true;
    }

    void Stop() {
        if (stopping_.exchange(true)) {
            return;
        }

        gathering_cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }

        std::shared_ptr<rtc::Track> track;
        std::shared_ptr<rtc::PeerConnection> peer_connection;
        std::string session_url;
        {
            std::lock_guard<std::mutex> lock(transport_mutex_);
            track = std::move(track_);
            peer_connection = std::move(peer_connection_);
            session_url = std::move(session_url_);
        }

        if (track != nullptr) {
            track->resetCallbacks();
            track->close();
        }
        if (peer_connection != nullptr) {
            peer_connection->resetCallbacks();
            peer_connection->close();
        }

        std::string shutdown_error;
        if (!StopWhepSession(session_url, shutdown_error) && !shutdown_error.empty()) {
            SetError(shutdown_error);
        }
        state_.store(DRIVER_STATION_RTC_STREAM_STOPPED);
    }

    DriverStationRtcResult SetPaused(bool paused) {
        if (stopping_.load()) {
            return DRIVER_STATION_RTC_INVALID_STATE;
        }
        const bool was_paused = paused_.load();
        if (was_paused == paused) {
            return DRIVER_STATION_RTC_SUCCESS;
        }

        if (paused) {
            paused_.store(true);
            latest_frame_.DiscardPending();
            if (state_.load() == DRIVER_STATION_RTC_STREAM_RUNNING) {
                state_.store(DRIVER_STATION_RTC_STREAM_PAUSED);
            }
            return DRIVER_STATION_RTC_SUCCESS;
        }

        std::string decoder_error;
        {
            std::lock_guard<std::mutex> lock(decoder_mutex_);
            if (!decoder_.Reset(decoder_error)) {
                SetFailure(decoder_error);
                return DRIVER_STATION_RTC_DECODE_ERROR;
            }
        }
        latest_frame_.DiscardPending();
        paused_.store(false);

        std::shared_ptr<rtc::Track> track;
        std::shared_ptr<rtc::PeerConnection> peer_connection;
        {
            std::lock_guard<std::mutex> lock(transport_mutex_);
            track = track_;
            peer_connection = peer_connection_;
        }
        if (track != nullptr && track->isOpen()) {
            track->requestKeyframe();
        }
        if (peer_connection != nullptr &&
            peer_connection->state() == rtc::PeerConnection::State::Connected) {
            state_.store(DRIVER_STATION_RTC_STREAM_RUNNING);
        } else if (state_.load() != DRIVER_STATION_RTC_STREAM_FAILED) {
            state_.store(DRIVER_STATION_RTC_STREAM_CONNECTING);
        }
        return DRIVER_STATION_RTC_SUCCESS;
    }

    DriverStationRtcResult GetNewestFrame(DriverStationRtcFrame& frame) {
        return latest_frame_.CopyNewest(frame);
    }

    DriverStationRtcStreamState State() const {
        return state_.load();
    }

    std::string Error() const {
        std::lock_guard<std::mutex> lock(error_mutex_);
        return error_;
    }

private:
    void Run() {
        try {
            rtc::Configuration configuration;
            configuration.disableAutoNegotiation = true;
            configuration.forceMediaTransport = true;

            auto peer_connection = std::make_shared<rtc::PeerConnection>(configuration);
            auto weak_self = weak_from_this();
            peer_connection->onStateChange([weak_self](rtc::PeerConnection::State state) {
                if (auto self = weak_self.lock()) {
                    self->OnPeerStateChanged(state);
                }
            });
            peer_connection->onGatheringStateChange(
                [weak_self](rtc::PeerConnection::GatheringState state) {
                    if (auto self = weak_self.lock()) {
                        if (state == rtc::PeerConnection::GatheringState::Complete) {
                            {
                                std::lock_guard<std::mutex> lock(self->gathering_mutex_);
                                self->gathering_complete_ = true;
                            }
                            self->gathering_cv_.notify_all();
                        }
                    }
                });

            rtc::Description::Video media(
                "video",
                rtc::Description::Direction::RecvOnly);
            media.addH264Codec(96);
            auto track = peer_connection->addTrack(media);

            auto depacketizer = std::make_shared<rtc::H264RtpDepacketizer>(
                rtc::NalUnit::Separator::LongStartSequence);
            depacketizer->addToChain(std::make_shared<rtc::RtcpReceivingSession>());
            track->setMediaHandler(depacketizer);
            track->onFrame([weak_self](rtc::binary data, rtc::FrameInfo info) {
                if (auto self = weak_self.lock()) {
                    self->OnEncodedFrame(std::move(data), std::move(info));
                }
            });
            track->onOpen([weak_self] {
                if (auto self = weak_self.lock()) {
                    self->RequestKeyframe();
                }
            });

            {
                std::lock_guard<std::mutex> lock(transport_mutex_);
                peer_connection_ = peer_connection;
                track_ = track;
            }

            peer_connection->setLocalDescription(rtc::Description::Type::Offer);
            {
                std::unique_lock<std::mutex> lock(gathering_mutex_);
                if (!gathering_cv_.wait_for(lock, kIceGatheringTimeout, [this] {
                        return gathering_complete_ || stopping_.load();
                    })) {
                    SetFailure("Timed out while gathering ICE candidates for WHEP");
                    return;
                }
            }
            if (stopping_.load()) {
                return;
            }

            const auto local_description = peer_connection->localDescription();
            if (!local_description.has_value()) {
                SetFailure("libdatachannel did not produce a WHEP SDP offer");
                return;
            }

            WhepSessionDescription session;
            std::string network_error;
            if (!StartWhepSession(
                    url_,
                    std::string(local_description.value()),
                    session,
                    network_error)) {
                SetFailure(network_error);
                return;
            }
            {
                std::lock_guard<std::mutex> lock(transport_mutex_);
                session_url_ = session.session_url;
            }
            if (stopping_.load()) {
                return;
            }

            peer_connection->setRemoteDescription(
                rtc::Description(session.answer_sdp, rtc::Description::Type::Answer));
        } catch (const std::exception& exception) {
            SetFailure(exception.what());
        } catch (...) {
            SetFailure("Unknown failure while starting the WHEP stream");
        }
    }

    void OnPeerStateChanged(rtc::PeerConnection::State state) {
        if (stopping_.load()) {
            return;
        }
        switch (state) {
        case rtc::PeerConnection::State::Connected:
            state_.store(paused_.load() ? DRIVER_STATION_RTC_STREAM_PAUSED
                                        : DRIVER_STATION_RTC_STREAM_RUNNING);
            RequestKeyframe();
            break;
        case rtc::PeerConnection::State::Failed:
            SetFailure("The WebRTC peer connection failed");
            break;
        case rtc::PeerConnection::State::Disconnected:
            state_.store(DRIVER_STATION_RTC_STREAM_CONNECTING);
            break;
        default:
            break;
        }
    }

    void OnEncodedFrame(rtc::binary data, rtc::FrameInfo info) {
        if (stopping_.load() || paused_.load() || data.empty()) {
            return;
        }

        std::uint64_t timestamp_us = 0;
        if (info.timestampSeconds.has_value()) {
            timestamp_us = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    info.timestampSeconds.value())
                    .count());
        }

        bool produced_frame = false;
        std::string decoder_error;
        {
            std::lock_guard<std::mutex> lock(decoder_mutex_);
            if (paused_.load() || stopping_.load()) {
                return;
            }
            if (!decoder_.Decode(
                    reinterpret_cast<const std::uint8_t*>(data.data()),
                    data.size(),
                    timestamp_us,
                    working_frame_,
                    produced_frame,
                    decoder_error)) {
                SetError(decoder_error);
            }
            if (produced_frame) {
                latest_frame_.Publish(working_frame_);
            }
        }

        if (!decoder_error.empty()) {
            RequestKeyframe();
        }
    }

    void RequestKeyframe() {
        std::shared_ptr<rtc::Track> track;
        {
            std::lock_guard<std::mutex> lock(transport_mutex_);
            track = track_;
        }
        if (track != nullptr && track->isOpen()) {
            track->requestKeyframe();
        }
    }

    void SetError(const std::string& error) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        error_ = error;
    }

    void SetFailure(const std::string& error) {
        if (stopping_.load()) {
            return;
        }
        SetError(error.empty() ? "The stream failed without an error message" : error);
        state_.store(DRIVER_STATION_RTC_STREAM_FAILED);
    }

    const std::string url_;
    std::atomic<DriverStationRtcStreamState> state_{
        DRIVER_STATION_RTC_STREAM_CONNECTING};
    std::atomic<bool> paused_{false};
    std::atomic<bool> stopping_{false};

    mutable std::mutex error_mutex_;
    std::string error_;

    std::mutex gathering_mutex_;
    std::condition_variable gathering_cv_;
    bool gathering_complete_ = false;

    std::mutex transport_mutex_;
    std::shared_ptr<rtc::PeerConnection> peer_connection_;
    std::shared_ptr<rtc::Track> track_;
    std::string session_url_;
    std::thread worker_;

    std::mutex decoder_mutex_;
    H264Decoder decoder_;
    DecodedFrame working_frame_;
    LatestFrameStore latest_frame_;
};

std::mutex g_module_mutex;
bool g_module_started = false;
bool g_module_stopping = false;
std::unordered_map<DriverStationRtcStream*, std::shared_ptr<StreamImpl>> g_streams;

std::shared_ptr<StreamImpl> FindStream(DriverStationRtcStream* handle) {
    if (handle == nullptr) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(g_module_mutex);
    const auto iterator = g_streams.find(handle);
    return iterator == g_streams.end() ? nullptr : iterator->second;
}

}  // namespace
}  // namespace driverstationrtc

extern "C" DriverStationRtcResult DriverStationRtc_StartModule(void) {
    try {
        std::lock_guard<std::mutex> lock(driverstationrtc::g_module_mutex);
        if (driverstationrtc::g_module_started) {
            return DRIVER_STATION_RTC_SUCCESS;
        }
        if (driverstationrtc::g_module_stopping) {
            return DRIVER_STATION_RTC_INVALID_STATE;
        }
        rtcPreload();
        driverstationrtc::g_module_started = true;
        return DRIVER_STATION_RTC_SUCCESS;
    } catch (...) {
        return DRIVER_STATION_RTC_INTERNAL_ERROR;
    }
}

extern "C" DriverStationRtcResult DriverStationRtc_StopModule(void) {
    try {
        std::vector<std::pair<DriverStationRtcStream*, std::shared_ptr<driverstationrtc::StreamImpl>>>
            streams;
        {
            std::lock_guard<std::mutex> lock(driverstationrtc::g_module_mutex);
            if (!driverstationrtc::g_module_started) {
                return DRIVER_STATION_RTC_SUCCESS;
            }
            driverstationrtc::g_module_started = false;
            driverstationrtc::g_module_stopping = true;
            streams.reserve(driverstationrtc::g_streams.size());
            for (auto& stream : driverstationrtc::g_streams) {
                streams.push_back(stream);
            }
            driverstationrtc::g_streams.clear();
        }

        for (auto& stream : streams) {
            stream.second->Stop();
            delete stream.first;
        }
        rtcCleanup();

        {
            std::lock_guard<std::mutex> lock(driverstationrtc::g_module_mutex);
            driverstationrtc::g_module_stopping = false;
        }
        return DRIVER_STATION_RTC_SUCCESS;
    } catch (...) {
        std::lock_guard<std::mutex> lock(driverstationrtc::g_module_mutex);
        driverstationrtc::g_module_stopping = false;
        return DRIVER_STATION_RTC_INTERNAL_ERROR;
    }
}

extern "C" DriverStationRtcResult DriverStationRtc_StartStream(
    const char* url,
    DriverStationRtcStream** stream) {
    if (url == nullptr || url[0] == '\0' || stream == nullptr) {
        return DRIVER_STATION_RTC_INVALID_ARGUMENT;
    }
    *stream = nullptr;

    try {
        auto implementation = std::make_shared<driverstationrtc::StreamImpl>(url);
        auto handle = std::make_unique<DriverStationRtcStream>();
        std::string error;
        {
            std::lock_guard<std::mutex> lock(driverstationrtc::g_module_mutex);
            if (!driverstationrtc::g_module_started || driverstationrtc::g_module_stopping) {
                return DRIVER_STATION_RTC_MODULE_NOT_STARTED;
            }
            if (!implementation->Start(error)) {
                return DRIVER_STATION_RTC_DECODE_ERROR;
            }
            driverstationrtc::g_streams.emplace(handle.get(), implementation);
        }

        *stream = handle.release();
        return DRIVER_STATION_RTC_SUCCESS;
    } catch (const std::bad_alloc&) {
        return DRIVER_STATION_RTC_OUT_OF_MEMORY;
    } catch (...) {
        return DRIVER_STATION_RTC_INTERNAL_ERROR;
    }
}

extern "C" DriverStationRtcResult DriverStationRtc_SetStreamPaused(
    DriverStationRtcStream* stream,
    int paused) {
    try {
        const auto implementation = driverstationrtc::FindStream(stream);
        return implementation == nullptr
                   ? DRIVER_STATION_RTC_INVALID_ARGUMENT
                   : implementation->SetPaused(paused != 0);
    } catch (...) {
        return DRIVER_STATION_RTC_INTERNAL_ERROR;
    }
}

extern "C" DriverStationRtcResult DriverStationRtc_StopStream(
    DriverStationRtcStream* stream) {
    if (stream == nullptr) {
        return DRIVER_STATION_RTC_INVALID_ARGUMENT;
    }

    try {
        std::shared_ptr<driverstationrtc::StreamImpl> implementation;
        {
            std::lock_guard<std::mutex> lock(driverstationrtc::g_module_mutex);
            const auto iterator = driverstationrtc::g_streams.find(stream);
            if (iterator == driverstationrtc::g_streams.end()) {
                return DRIVER_STATION_RTC_INVALID_ARGUMENT;
            }
            implementation = std::move(iterator->second);
            driverstationrtc::g_streams.erase(iterator);
        }
        implementation->Stop();
        delete stream;
        return DRIVER_STATION_RTC_SUCCESS;
    } catch (...) {
        return DRIVER_STATION_RTC_INTERNAL_ERROR;
    }
}

extern "C" DriverStationRtcStreamState DriverStationRtc_GetStreamState(
    DriverStationRtcStream* stream) {
    try {
        const auto implementation = driverstationrtc::FindStream(stream);
        return implementation == nullptr ? DRIVER_STATION_RTC_STREAM_STOPPED
                                         : implementation->State();
    } catch (...) {
        return DRIVER_STATION_RTC_STREAM_FAILED;
    }
}

extern "C" const char* DriverStationRtc_GetStreamError(
    DriverStationRtcStream* stream) {
    static thread_local std::string error;
    try {
        const auto implementation = driverstationrtc::FindStream(stream);
        error = implementation == nullptr ? "Invalid stream handle" : implementation->Error();
    } catch (...) {
        error = "Failed to read the stream error";
    }
    return error.c_str();
}

extern "C" DriverStationRtcResult DriverStationRtc_GetNewestFrame(
    DriverStationRtcStream* stream,
    DriverStationRtcFrame* frame) {
    if (frame == nullptr) {
        return DRIVER_STATION_RTC_INVALID_ARGUMENT;
    }
    try {
        const auto implementation = driverstationrtc::FindStream(stream);
        return implementation == nullptr
                   ? DRIVER_STATION_RTC_INVALID_ARGUMENT
                   : implementation->GetNewestFrame(*frame);
    } catch (const std::bad_alloc&) {
        return DRIVER_STATION_RTC_OUT_OF_MEMORY;
    } catch (...) {
        return DRIVER_STATION_RTC_INTERNAL_ERROR;
    }
}

extern "C" void DriverStationRtc_FreeFrame(DriverStationRtcFrame* frame) {
    if (frame != nullptr) {
        driverstationrtc::ReleaseFrame(*frame);
    }
}
