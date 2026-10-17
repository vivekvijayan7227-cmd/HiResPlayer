cmake_minimum_required(VERSION 3.20)
project(HiResAudioEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT DEFINED FLAC_ROOT)
    get_filename_component(DEFAULT_FLAC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../flac-1.4.3" ABSOLUTE)
    if (EXISTS "${DEFAULT_FLAC_ROOT}")
        set(FLAC_ROOT "${DEFAULT_FLAC_ROOT}")
    else()
        set(FLAC_ROOT "d:/AudioPlayer/flac-1.4.3")
    endif()
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FLAC_ROOT}/include
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
    ${FLAC_ROOT}/build/src/libFLAC/libFLAC.a
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
