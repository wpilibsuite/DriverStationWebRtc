function(_driverstationrtc_append_license output_file heading source_file)
    if(NOT EXISTS "${source_file}")
        message(FATAL_ERROR
            "Missing license source for ${heading}: ${source_file}")
    endif()

    file(READ "${source_file}" license_text)
    file(APPEND "${output_file}"
        "\n\n===============================================================================\n"
        "${heading}\n"
        "===============================================================================\n\n"
        "${license_text}"
    )
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${source_file}")
endfunction()

function(driverstationrtc_generate_license_bundle output_file openh264_binary_license)
    file(WRITE "${output_file}"
        "DriverStationRtcClient license bundle\n"
        "=======================================\n\n"
        "This file contains the project license and the complete license texts for\n"
        "all bundled, built, or redistributed dependencies. See\n"
        "THIRD_PARTY_NOTICES.md in the source and installed package for the component\n"
        "inventory and how each component is used.\n"
    )

    _driverstationrtc_append_license(
        "${output_file}"
        "DriverStationRtcClient -- BSD-3-Clause"
        "${PROJECT_SOURCE_DIR}/LICENSE"
    )
    _driverstationrtc_append_license(
        "${output_file}"
        "Mbed TLS, mbedtls-framework, and p256-m -- Apache-2.0 OR GPL-2.0-or-later; Project Everest -- Apache-2.0"
        "${PROJECT_SOURCE_DIR}/mbedtls/LICENSE"
    )
    _driverstationrtc_append_license(
        "${output_file}"
        "libdatachannel and libjuice -- MPL-2.0"
        "${PROJECT_SOURCE_DIR}/libdatachannel/LICENSE"
    )
    _driverstationrtc_append_license(
        "${output_file}"
        "libjuice picohash implementation -- public-domain notice"
        "${PROJECT_SOURCE_DIR}/licenses/LibjuicePicohash.txt"
    )
    _driverstationrtc_append_license(
        "${output_file}"
        "libsrtp -- BSD-3-Clause"
        "${PROJECT_SOURCE_DIR}/libdatachannel/deps/libsrtp/LICENSE"
    )
    _driverstationrtc_append_license(
        "${output_file}"
        "usrsctp -- BSD-3-Clause"
        "${PROJECT_SOURCE_DIR}/libdatachannel/deps/usrsctp/LICENSE.md"
    )
    _driverstationrtc_append_license(
        "${output_file}"
        "usrsctp sctp_ss_functions.c -- BSD-2-Clause"
        "${PROJECT_SOURCE_DIR}/licenses/UsrsctpSctpSsFunctions.txt"
    )
    _driverstationrtc_append_license(
        "${output_file}"
        "plog -- MIT"
        "${PROJECT_SOURCE_DIR}/libdatachannel/deps/plog/LICENSE"
    )
    _driverstationrtc_append_license(
        "${output_file}"
        "nlohmann/json -- MIT (bundled but not linked by the current build)"
        "${PROJECT_SOURCE_DIR}/libdatachannel/deps/json/LICENSE.MIT"
    )
    _driverstationrtc_append_license(
        "${output_file}"
        "OpenH264 API headers -- BSD-2-Clause"
        "${PROJECT_SOURCE_DIR}/licenses/OpenH264Source.txt"
    )
    _driverstationrtc_append_license(
        "${output_file}"
        "Cisco-provided OpenH264 binary -- binary, BSD, and AVC/H.264 patent terms"
        "${openh264_binary_license}"
    )
endfunction()
