cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_grbsystems CXX C)

LIST(APPEND CMAKE_MODULE_PATH "~/Projects/indi/cmake_modules")
include(GNUInstallDirs)

if (NOT WIN32 AND NOT ANDROID)
set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
endif(NOT WIN32 AND NOT ANDROID)

find_package(INDI REQUIRED)

set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")
set(RULES_INSTALL_DIR "/etc/udev/rules.d")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_fusion_focus.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_fusion_focus.xml )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})

########### GRBSystems ###########
set(indifusionfocus_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/fusion-focus.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/fusion-focus-driver.cpp
   )

add_executable(indi_fusion_focus ${indifusionfocus_SRCS})

target_link_libraries(indi_fusion_focus ${INDI_LIBRARIES} pthread)

install(TARGETS indi_fusion_focus RUNTIME DESTINATION bin )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_fusion_focus.xml DESTINATION ${INDI_DATA_DIR})
