cmake_minimum_required(VERSION 3.22.1)
project(dubsiren CXX C)

# Shared host core (firmware sources, stubs, AAudio output) in app/native.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
add_subdirectory("${REPO_ROOT}/app/native" "${CMAKE_CURRENT_BINARY_DIR}/siren_core")

# JNI surface for the Kotlin app.
add_library(dubsiren SHARED siren_bridge.cpp)
target_compile_features(dubsiren PRIVATE cxx_std_17)
target_compile_options(dubsiren PRIVATE -O2)
target_link_libraries(dubsiren PRIVATE siren_core)
