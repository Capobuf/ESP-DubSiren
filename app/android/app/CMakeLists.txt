cmake_minimum_required(VERSION 3.22.1)
project(dubsiren CXX C)

# libsiren_core.so for dart:ffi (lib/siren_native.dart): the shared core in
# app/native, with the AAudio output.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../native" "${CMAKE_CURRENT_BINARY_DIR}/siren_core")
