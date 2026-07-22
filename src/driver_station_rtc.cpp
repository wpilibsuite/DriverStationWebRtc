#include "driver_station_rtc.h"

#include <cstdio>

#include <mbedtls/version.h>
#include <rtc/rtc.h>
#include <wels/codec_api.h>

extern "C" const char* DriverStationRtc_GetDependencyVersions(void) {
    static thread_local char versions[192];
    char mbedtls_version[18];
    OpenH264Version openh264_version{};
    mbedtls_version_get_string_full(mbedtls_version);
    WelsGetCodecVersionEx(&openh264_version);

    std::snprintf(
        versions,
        sizeof(versions),
        "libdatachannel %s; %s; OpenH264 %u.%u.%u",
        RTC_VERSION,
        mbedtls_version,
        openh264_version.uMajor,
        openh264_version.uMinor,
        openh264_version.uRevision);
    return versions;
}

extern "C" int DriverStationRtc_RunDependencySmokeTest(void) {
    rtcPreload();
    rtcCleanup();

    OpenH264Version openh264_version{};
    WelsGetCodecVersionEx(&openh264_version);
    if (openh264_version.uMajor == 0 || mbedtls_version_get_number() == 0) {
        return 1;
    }
    return 0;
}
