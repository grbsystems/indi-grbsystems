cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_grbsystems CXX C)

LIST(APPEND CMAKE_MODULE_PATH "~/Projects/indi/cmake_modules")
include(GNUInstallDirs)

if (NOT WIN32 AND NOT ANDROID)
set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
endif(NOT WIN32 AND NOT ANDROID)

find_package(INDI REQUIRED)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})

option(WITH_HID_FOCUS "Install Hid Focus" On)
option(WITH_FUSION_FOCUS "Install Fusion Focus" Off)

if (WITH_HID_FOCUS)
add_subdirectory(hid-focus)
endif(WITH_HID_FOCUS)

if (WITH_FUSION_FOCUS)
add_subdirectory(fusion-focus)
endif(WITH_FUSION_FOCUS)
