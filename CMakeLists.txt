cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Configure a separate build directory.")
endif()

project(DriverStationRtcClient VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CheckSymbolExists)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# Keep the runtime consistent across the wrapper and every dependency added below.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING
    "MSVC runtime library" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if(MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()

# Mbed TLS is an in-tree, static-only dependency.
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
set(GEN_FILES OFF CACHE BOOL "Generate Mbed TLS source files" FORCE)
set(MBEDTLS_USER_CONFIG_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DriverStationRtcMbedTlsConfig.h"
    CACHE FILEPATH "Mbed TLS options required by libdatachannel" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
set(DISABLE_PACKAGE_CONFIG_AND_INSTALL ON CACHE BOOL "" FORCE)
set(MSVC_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
add_subdirectory(mbedtls EXCLUDE_FROM_ALL)

# libdatachannel expects this aggregate target when Mbed TLS is embedded as a
# subproject. Mbed TLS itself exposes the lower-case component targets.
add_library(MbedTLS::MbedTLS INTERFACE IMPORTED GLOBAL)
set_target_properties(MbedTLS::MbedTLS PROPERTIES
    INTERFACE_LINK_LIBRARIES MbedTLS::mbedtls
)

# libdatachannel and all of its bundled dependencies are static libraries.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_DEPS_LIBS OFF CACHE BOOL "" FORCE)
set(USE_MBEDTLS ON CACHE BOOL "" FORCE)
set(USE_GNUTLS OFF CACHE BOOL "" FORCE)
set(PREFER_SYSTEM_LIB OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_SRTP OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_JUICE OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_USRSCTP OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_PLOG OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_JSON OFF CACHE BOOL "" FORCE)
set(NO_MEDIA OFF CACHE BOOL "" FORCE)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
set(WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(BUILD_WITH_WARNINGS OFF CACHE BOOL "" FORCE)
set(ENABLE_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(RTC_UPDATE_VERSION_HEADER OFF CACHE BOOL "" FORCE)
# libdatachannel uses Mbed TLS for DTLS. libSRTP uses its bundled crypto
# implementation so it does not independently rediscover Mbed TLS.
set(ENABLE_MBEDTLS OFF CACHE BOOL "libSRTP Mbed TLS crypto backend" FORCE)
set(ENABLE_OPENSSL OFF CACHE BOOL "libSRTP OpenSSL crypto backend" FORCE)
set(MBEDTLS OFF CACHE INTERNAL "libSRTP Mbed TLS crypto backend")
set(GCM OFF CACHE INTERNAL "libSRTP GCM backend")
add_subdirectory(libdatachannel EXCLUDE_FROM_ALL)

include(cmake/OpenH264.cmake)
driverstationrtc_add_openh264()

include(cmake/GenerateLicenseBundle.cmake)
set(DRIVER_STATION_RTC_LICENSE_BUNDLE
    "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt")
driverstationrtc_generate_license_bundle(
    "${DRIVER_STATION_RTC_LICENSE_BUNDLE}"
    "${OPENH264_BINARY_LICENSE_FILE}"
)

add_library(DriverStationRtc SHARED
    src/driver_station_rtc.cpp
    src/frame_conversion.cpp
    src/h264_decoder.cpp
    src/latest_frame_store.cpp
    src/whep_http.cpp
)
add_library(DriverStationRtc::DriverStationRtc ALIAS DriverStationRtc)
target_compile_features(DriverStationRtc PRIVATE cxx_std_17)
target_compile_definitions(DriverStationRtc PRIVATE DRIVER_STATION_RTC_BUILDING_LIBRARY)
target_include_directories(DriverStationRtc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(DriverStationRtc
    PRIVATE
        LibDataChannel::LibDataChannel
        MbedTLS::mbedtls
        OpenH264::OpenH264
)
set_target_properties(DriverStationRtc PROPERTIES
    PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/driver_station_rtc.h"
)

include(cmake/GenerateExports.cmake)
driverstationrtc_generate_exports(DriverStationRtc "${CMAKE_CURRENT_SOURCE_DIR}/exports.txt")

if(MSVC)
    # LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS.
    # Apply this directly to the public DLL instead of relying on callers to
    # populate the global CMAKE_SHARED_LINKER_FLAGS cache entry.
    target_link_options(DriverStationRtc PRIVATE "/DEPENDENTLOADFLAG:0x1100")
endif()

add_custom_command(TARGET DriverStationRtc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:OpenH264::OpenH264>"
        "$<TARGET_FILE_DIR:DriverStationRtc>"
    VERBATIM
)

if(APPLE)
    get_filename_component(openh264_runtime_name "${OPENH264_RUNTIME_FILE}" NAME)
    set(openh264_install_name "/usr/local/lib/${openh264_runtime_name}")
    set_target_properties(DriverStationRtc PROPERTIES
        BUILD_RPATH "@loader_path"
        INSTALL_RPATH "@loader_path"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
    # Cisco's dylib has an absolute /usr/local/lib install name. Make the
    # wrapper resolve it from its own directory in both build and install trees.
    add_custom_command(TARGET DriverStationRtc POST_BUILD
        COMMAND "${CMAKE_INSTALL_NAME_TOOL}" -change
            "${openh264_install_name}"
            "@loader_path/${openh264_runtime_name}"
            "$<TARGET_FILE:DriverStationRtc>"
        VERBATIM
    )
elseif(UNIX)
    set_target_properties(DriverStationRtc PROPERTIES
        BUILD_RPATH "\$ORIGIN"
        INSTALL_RPATH "\$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

add_executable(DriverStationRtcSmoke src/main.c)
target_compile_features(DriverStationRtcSmoke PRIVATE c_std_99)
target_link_libraries(DriverStationRtcSmoke PRIVATE DriverStationRtc::DriverStationRtc)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME DriverStationRtc.LinkSmoke COMMAND DriverStationRtcSmoke)

    add_executable(DriverStationRtcFrameTest
        tests/frame_test.cpp
        src/frame_conversion.cpp
        src/latest_frame_store.cpp
    )
    target_compile_features(DriverStationRtcFrameTest PRIVATE cxx_std_17)
    target_include_directories(DriverStationRtcFrameTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME DriverStationRtc.FrameBuffer COMMAND DriverStationRtcFrameTest)
endif()

install(TARGETS DriverStationRtc
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(WIN32)
    install(FILES "${OPENH264_RUNTIME_FILE}" DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    install(FILES "${OPENH264_RUNTIME_FILE}" DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
install(FILES
    "${DRIVER_STATION_RTC_LICENSE_BUNDLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/DriverStationRtcClient"
)
