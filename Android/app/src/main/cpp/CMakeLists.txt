cmake_minimum_required(VERSION 3.22.1)
project(dubsiren CXX C)

# Shared firmware sources, compiled unmodified from the repository root.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(FIRMWARE "${REPO_ROOT}/firmware")

file(GLOB FIRMWARE_SOURCES CONFIGURE_DEPENDS
    "${FIRMWARE}/src/audio/*.cpp"
    "${FIRMWARE}/src/control/*.cpp"
    "${FIRMWARE}/src/display/*.cpp"
)

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

add_library(dubsiren SHARED
    siren_bridge.cpp
    stubs/Adafruit_ST7789.cpp
    "${adafruit_gfx_SOURCE_DIR}/Adafruit_GFX.cpp"
    ${FIRMWARE_SOURCES}
)

# stubs/ first: Arduino.h, Print.h, SPI.h and Adafruit_ST7789.h replace the
# ESP32 SDK; the GFX folder also provides Fonts/ for the display.
target_include_directories(dubsiren PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
    "${adafruit_gfx_SOURCE_DIR}"
    "${FIRMWARE}/src"
    "${FIRMWARE}/include"
)

target_compile_features(dubsiren PRIVATE cxx_std_17)
# Arduino 1.x API level: Adafruit_GFX.h tests it before including Arduino.h.
target_compile_definitions(dubsiren PRIVATE ARDUINO=10819)
# Plain IEEE float like the firmware build: no fast-math reassociation.
target_compile_options(dubsiren PRIVATE -O2)
target_link_libraries(dubsiren PRIVATE aaudio log)
