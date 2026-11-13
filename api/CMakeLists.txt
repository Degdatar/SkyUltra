cmake_minimum_required(VERSION 3.25)
project(SkyUltraAPI VERSION 0.4.0 LANGUAGES C CXX)

add_library(skyultra_api INTERFACE)
add_library(SkyUltra::API ALIAS skyultra_api)

target_include_directories(skyultra_api INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(skyultra_api INTERFACE cxx_std_20)
