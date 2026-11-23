cmake_minimum_required(VERSION 3.22.1)

# siren_core: the shared firmware sources (compiled unmodified), the host stubs
# and the C surface in siren_core.h, plus the platform audio output. Added
# with add_subdirectory() by the Flutter Windows and Android builds.

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(FIRMWARE "${REPO_ROOT}/firmware")

file(GLOB FIRMWARE_SOURCES CONFIGURE_DEPENDS
    "${FIRMWARE}/src/audio/*.cpp"
    "${FIRMWARE}/src/control/*.cpp"
    "${FIRMWARE}/src/display/*.cpp"
)
# The panel driver (esp_lcd, SPI DMA) becomes a framebuffer: stubs/PanelLcd.cpp.
list(REMOVE_ITEM FIRMWARE_SOURCES "${FIRMWARE}/src/display/PanelLcd.cpp")

# Adafruit GFX at the version pinned in firmware/platformio.ini. Only the
# sources are fetched: its own CMakeLists.txt is an ESP-IDF component.
include(FetchContent)
FetchContent_Declare(adafruit_gfx
    URL https://github.com/adafruit/Adafruit-GFX-Library/archive/refs/tags/1.12.6.tar.gz
    URL_HASH SHA256=3874218e5b9947ea11132cea5d6e8945f48306a5dada5b5b52f8f9ded53ee88f
)
FetchContent_GetProperties(adafruit_gfx)
if(NOT adafruit_gfx_POPULATED)
    FetchContent_Populate(adafruit_gfx)
endif()

# siren_core.dll / libsiren_core.so, loaded by lib/siren_native.dart (dart:ffi).
add_library(siren_core SHARED
    siren_core.cpp
    stubs/PanelLcd.cpp
    "${adafruit_gfx_SOURCE_DIR}/Adafruit_GFX.cpp"
    ${FIRMWARE_SOURCES}
)
if(ANDROID)
    target_sources(siren_core PRIVATE audio_aaudio.cpp)
    target_link_libraries(siren_core PUBLIC aaudio log)
elseif(WIN32)
    # miniaudio single header; public domain or MIT No Attribution.
    FetchContent_Declare(miniaudio
        URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.26.tar.gz
        URL_HASH SHA256=838ba55a21c9142adf7b76bdc4351e5cd8b7ae128f46b25ec25f94ae9969c600
    )
    FetchContent_GetProperties(miniaudio)
    if(NOT miniaudio_POPULATED)
        FetchContent_Populate(miniaudio)
    endif()
    target_sources(siren_core PRIVATE audio_miniaudio.cpp)
    target_include_directories(siren_core PRIVATE "${miniaudio_SOURCE_DIR}")
endif()

# siren_core.h is the public surface; stubs/ first: Arduino.h, Print.h and the
# BusIO headers replace the ESP32 SDK; the GFX folder also provides Fonts/.
target_include_directories(siren_core PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_include_directories(siren_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/stubs"
    "${adafruit_gfx_SOURCE_DIR}"
    "${FIRMWARE}/src"
    "${FIRMWARE}/include"
)

target_compile_features(siren_core PRIVATE cxx_std_17)
# Arduino 1.x API level: Adafruit_GFX.h tests it before including Arduino.h.
target_compile_definitions(siren_core PRIVATE ARDUINO=10819)
# Plain IEEE float like the firmware build: no fast-math reassociation
# (MSVC: /fp:precise is the default, /O2 comes with the Release config).
if(MSVC)
    # The firmware and GFX use the C library as on the ESP32 (sscanf, strcpy).
    target_compile_definitions(siren_core PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(siren_core PRIVATE -O2)
endif()
