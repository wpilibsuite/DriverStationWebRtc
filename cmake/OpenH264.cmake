set(OPENH264_VERSION "2.6.0" CACHE STRING "OpenH264 binary release version")

function(_driverstationrtc_download url output_file)
    if(EXISTS "${output_file}")
        return()
    endif()

    get_filename_component(output_dir "${output_file}" DIRECTORY)
    file(MAKE_DIRECTORY "${output_dir}")
    file(DOWNLOAD "${url}" "${output_file}"
        STATUS download_status
        LOG download_log
        SHOW_PROGRESS
        TLS_VERIFY ON
    )
    list(GET download_status 0 status_code)
    list(GET download_status 1 status_message)
    if(NOT status_code EQUAL 0)
        file(REMOVE "${output_file}")
        message(FATAL_ERROR
            "Failed to download ${url}: ${status_message}\n${download_log}")
    endif()
endfunction()

function(driverstationrtc_add_openh264)
    set(binary_base_url "https://ciscobinary.openh264.org")
    set(source_base_url
        "https://raw.githubusercontent.com/cisco/openh264/v${OPENH264_VERSION}/codec/api/wels")
    set(root_dir "${CMAKE_BINARY_DIR}/_deps/openh264-${OPENH264_VERSION}")
    set(download_dir "${root_dir}/downloads")
    set(include_dir "${root_dir}/include")
    file(MAKE_DIRECTORY "${download_dir}" "${include_dir}/wels")

    foreach(header codec_api.h codec_app_def.h codec_def.h codec_ver.h)
        _driverstationrtc_download(
            "${source_base_url}/${header}"
            "${include_dir}/wels/${header}"
        )
    endforeach()

    _driverstationrtc_download(
        "https://www.openh264.org/BINARY_LICENSE.txt"
        "${root_dir}/BINARY_LICENSE.txt"
    )

    if(WIN32)
        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(asset_name "openh264-${OPENH264_VERSION}-win32.dll.bz2")
            set(runtime_name "openh264.dll")
            set(machine X86)
        elseif(CMAKE_C_COMPILER_ARCHITECTURE_ID MATCHES "^(ARM64|arm64|aarch64)$")
            set(asset_name "openh264-${OPENH264_VERSION}-win-arm64.dll.bz2")
            set(runtime_name "openh264.dll")
            set(machine ARM64)
        else()
            set(asset_name "openh264-${OPENH264_VERSION}-win64.dll.bz2")
            set(runtime_name "openh264.dll")
            set(machine X64)
        endif()
    elseif(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
            set(asset_name "libopenh264-${OPENH264_VERSION}-mac-arm64.dylib.bz2")
        else()
            set(asset_name "libopenh264-${OPENH264_VERSION}-mac-x64.dylib.bz2")
        endif()
        # This matches the LC_ID_DYLIB embedded in Cisco's prebuilt binary.
        set(runtime_name "libopenh264.8.dylib")
    elseif(ANDROID)
        if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
            set(asset_arch arm64)
        elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
            set(asset_arch arm)
        elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
            set(asset_arch x64)
        elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86")
            set(asset_arch x86)
        else()
            message(FATAL_ERROR "Unsupported Android ABI for OpenH264: ${CMAKE_ANDROID_ARCH_ABI}")
        endif()
        set(asset_name "libopenh264-${OPENH264_VERSION}-android-${asset_arch}.8.so.bz2")
        set(runtime_name "libopenh264.so")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
            set(asset_arch -arm64)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM|arm)$")
            set(asset_arch -arm)
        elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(asset_arch 32)
        else()
            set(asset_arch 64)
        endif()
        set(asset_name "libopenh264-${OPENH264_VERSION}-linux${asset_arch}.8.so.bz2")
        set(runtime_name "libopenh264.so.8")
    else()
        message(FATAL_ERROR "No OpenH264 binary mapping for ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    string(REGEX REPLACE "\\.bz2$" "" checksum_asset_name "${asset_name}")
    # Keep runtime products architecture-specific even if multiple targets are
    # configured below the same parent build directory.
    set(runtime_dir "${root_dir}/${checksum_asset_name}/bin")
    file(MAKE_DIRECTORY "${runtime_dir}")
    set(archive_file "${download_dir}/${asset_name}")
    set(checksum_file "${download_dir}/${checksum_asset_name}.signed.md5.txt")
    set(runtime_file "${runtime_dir}/${runtime_name}")
    _driverstationrtc_download(
        "${binary_base_url}/${checksum_asset_name}.signed.md5.txt"
        "${checksum_file}"
    )
    _driverstationrtc_download("${binary_base_url}/${asset_name}" "${archive_file}")

    if(NOT EXISTS "${runtime_file}")
        set(extract_dir "${root_dir}/extract")
        file(REMOVE_RECURSE "${extract_dir}")
        file(MAKE_DIRECTORY "${extract_dir}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E tar xjf "${archive_file}"
            WORKING_DIRECTORY "${extract_dir}"
            RESULT_VARIABLE extract_result
            ERROR_VARIABLE extract_error
        )
        if(extract_result EQUAL 0)
            file(GLOB extracted_files LIST_DIRECTORIES FALSE "${extract_dir}/*")
            list(LENGTH extracted_files extracted_count)
            if(NOT extracted_count EQUAL 1)
                message(FATAL_ERROR
                    "Expected one file in ${asset_name}, found ${extracted_count}")
            endif()
            list(GET extracted_files 0 extracted_file)
            file(RENAME "${extracted_file}" "${runtime_file}")
        else()
            find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
            execute_process(
                COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/DecompressBzip2.py"
                    "${archive_file}" "${runtime_file}"
                RESULT_VARIABLE python_extract_result
                ERROR_VARIABLE python_extract_error
            )
            if(NOT python_extract_result EQUAL 0)
                message(FATAL_ERROR
                    "Failed to decompress ${asset_name}: ${extract_error}\n${python_extract_error}")
            endif()
        endif()
        file(REMOVE_RECURSE "${extract_dir}")
    endif()

    file(READ "${checksum_file}" checksum_contents)
    string(FIND "${checksum_contents}" "${checksum_asset_name}" checksum_name_index)
    if(checksum_name_index EQUAL -1)
        message(FATAL_ERROR
            "Cisco's checksum does not name the expected file ${checksum_asset_name}")
    endif()

    # Cisco publishes GNU md5sum format for Windows and BSD md5 format for
    # Unix binaries. Accept both representations while still requiring the
    # expected asset name and a full 128-bit digest.
    if(checksum_contents MATCHES "^([0-9A-Fa-f]+)[ \t]+\\*?")
        set(expected_md5 "${CMAKE_MATCH_1}")
    elseif(checksum_contents MATCHES "=[ \t]*([0-9A-Fa-f]+)")
        set(expected_md5 "${CMAKE_MATCH_1}")
    else()
        set(expected_md5 "")
    endif()
    string(LENGTH "${expected_md5}" expected_md5_length)
    if(NOT expected_md5_length EQUAL 32)
        message(FATAL_ERROR "Could not parse Cisco's checksum for ${checksum_asset_name}")
    endif()
    file(MD5 "${runtime_file}" actual_md5)
    string(TOLOWER "${expected_md5}" expected_md5)
    string(TOLOWER "${actual_md5}" actual_md5)
    if(NOT actual_md5 STREQUAL expected_md5)
        file(REMOVE "${archive_file}" "${runtime_file}")
        message(FATAL_ERROR
            "Checksum mismatch for ${checksum_asset_name}: expected ${expected_md5}, got ${actual_md5}")
    endif()

    if(WIN32)
        set(def_file "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/OpenH264.def")
        set(import_library "${runtime_dir}/openh264.lib")
        add_custom_command(
            OUTPUT "${import_library}"
            COMMAND "${CMAKE_AR}" /NOLOGO /MACHINE:${machine}
                "/DEF:${def_file}" "/OUT:${import_library}"
            DEPENDS "${runtime_file}" "${def_file}"
            VERBATIM
        )
        add_custom_target(driverstationrtc_openh264_import_library DEPENDS "${import_library}")

        add_library(OpenH264::OpenH264 SHARED IMPORTED GLOBAL)
        set_target_properties(OpenH264::OpenH264 PROPERTIES
            IMPORTED_LOCATION "${runtime_file}"
            IMPORTED_IMPLIB "${import_library}"
            INTERFACE_INCLUDE_DIRECTORIES "${include_dir}"
        )
        add_dependencies(OpenH264::OpenH264 driverstationrtc_openh264_import_library)
    else()
        add_library(OpenH264::OpenH264 SHARED IMPORTED GLOBAL)
        set_target_properties(OpenH264::OpenH264 PROPERTIES
            IMPORTED_LOCATION "${runtime_file}"
            INTERFACE_INCLUDE_DIRECTORIES "${include_dir}"
        )
    endif()

    set(OPENH264_RUNTIME_FILE "${runtime_file}" PARENT_SCOPE)
    set(OPENH264_BINARY_LICENSE_FILE "${root_dir}/BINARY_LICENSE.txt" PARENT_SCOPE)
endfunction()
