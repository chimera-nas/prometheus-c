# SPDX-FileCopyrightText: 2025 Ben Jarvis
# SPDX-License-Identifier: LGPL-2.1-only
cmake_minimum_required(VERSION 3.18)
project(prometheus-c LANGUAGES C)
enable_testing()
find_package(Threads REQUIRED)
add_library(prometheus-c SHARED prometheus-c.c prometheus-c.h)
target_compile_features(prometheus-c PUBLIC c_std_11)
target_compile_definitions(prometheus-c PRIVATE PROMETHEUS_BUILD)
if(MSVC)
    target_compile_options(prometheus-c PRIVATE /W3 /WX)
    target_compile_definitions(prometheus-c PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(prometheus-c PRIVATE -Wall -Werror -fvisibility=hidden)
    target_link_libraries(prometheus-c PRIVATE m)
endif()
target_link_libraries(prometheus-c PRIVATE Threads::Threads)
target_include_directories(prometheus-c PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ext/stopwatch>
    $<INSTALL_INTERFACE:include>)
install(TARGETS prometheus-c RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(FILES prometheus-c.h ext/stopwatch/stopwatch.h DESTINATION include)
add_subdirectory(tests)
