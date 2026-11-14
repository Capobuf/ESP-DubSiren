cmake_minimum_required(VERSION 3.22.1)
project(dubsiren CXX C)

# Shared firmware sources, compiled unmodified from the repository root.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(FIRMWARE "${REPO_ROOT}/firmware")

file(GLOB FIRMWARE_SOURCES CONFIGURE_DEPENDS
    "${FIRMWARE}/src/audio/*.cpp"
    "${FIRMWARE}/src/control/*.cpp"
)

add_library(dubsiren SHARED
    siren_bridge.cpp
    stubs/Adafruit_ST7789.cpp
    ${FIRMWARE_SOURCES}
    "${FIRMWARE}/src/display/PanelDisplay.cpp"
)

# stubs/ first: Arduino.h, SPI.h and Adafruit_ST7789.h replace the ESP32 SDK.
target_include_directories(dubsiren PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
    "${FIRMWARE}/src"
    "${FIRMWARE}/include"
)

target_compile_features(dubsiren PRIVATE cxx_std_17)
# Plain IEEE float like the firmware build: no fast-math reassociation.
target_compile_options(dubsiren PRIVATE -O2)
target_link_libraries(dubsiren PRIVATE aaudio log)
