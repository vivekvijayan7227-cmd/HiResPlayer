cmake_minimum_required(VERSION 3.20)
project(HiResAudioEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Portable libFLAC Discovery ────────────────────────────────────────────────
if (NOT DEFINED FLAC_ROOT)
    if (DEFINED ENV{FLAC_ROOT} AND EXISTS "$ENV{FLAC_ROOT}")
        set(FLAC_ROOT "$ENV{FLAC_ROOT}")
    else()
        get_filename_component(CANDIDATE_1 "${CMAKE_CURRENT_SOURCE_DIR}/../../../flac-1.4.3" ABSOLUTE)
        get_filename_component(CANDIDATE_2 "${CMAKE_CURRENT_SOURCE_DIR}/../../flac-1.4.3" ABSOLUTE)
        get_filename_component(CANDIDATE_3 "${CMAKE_CURRENT_SOURCE_DIR}/../flac-1.4.3" ABSOLUTE)
        if (EXISTS "${CANDIDATE_1}")
            set(FLAC_ROOT "${CANDIDATE_1}")
        elseif (EXISTS "${CANDIDATE_2}")
            set(FLAC_ROOT "${CANDIDATE_2}")
        elseif (EXISTS "${CANDIDATE_3}")
            set(FLAC_ROOT "${CANDIDATE_3}")
        endif()
    endif()
endif()

find_path(FLAC_INCLUDE_DIR NAMES FLAC/stream_decoder.h
    HINTS
    ${FLAC_ROOT}/include
    $ENV{FLAC_ROOT}/include
)

find_library(FLAC_LIBRARY NAMES FLAC libFLAC libFLAC.a libFLAC.lib
    HINTS
    ${FLAC_ROOT}/build/src/libFLAC
    ${FLAC_ROOT}/lib
    ${FLAC_ROOT}/build/lib
    $ENV{FLAC_ROOT}/build/src/libFLAC
    $ENV{FLAC_ROOT}/lib
)

if (NOT FLAC_INCLUDE_DIR OR NOT FLAC_LIBRARY)
    message(FATAL_ERROR "libFLAC not found! Please set FLAC_ROOT environment variable or pass -DFLAC_ROOT=<path> to CMake.")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FLAC_INCLUDE_DIR}
)

set(ENGINE_SOURCES
    src/AudioDeviceManager.cpp
    src/AudioFormatNegotiator.cpp
    src/AudioDiagnostics.cpp
    src/FlacDecoder.cpp
    src/WasapiRenderer.cpp
    src/PlaybackController.cpp
    src/BluetoothDeviceService.cpp
    src/hires_audio_api.cpp
)

add_library(HiResAudioEngine SHARED ${ENGINE_SOURCES})

target_compile_definitions(HiResAudioEngine PRIVATE HP_EXPORTS NOMINMAX FLAC__NO_DLL)

target_link_libraries(HiResAudioEngine PRIVATE
    ${FLAC_LIBRARY}
    ole32
    oleaut32
    avrt
    ksuser
    uuid
)

set_target_properties(HiResAudioEngine PROPERTIES
    OUTPUT_NAME "HiResAudioEngine"
    PREFIX ""
)
