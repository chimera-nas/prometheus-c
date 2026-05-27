# SPDX-FileCopyrightText: 2025 Ben Jarvis
#
# SPDX-License-Identifier: LGPL-2.1-only

project(prometheus-c LANGUAGES C)

add_definitions(-O3 -Wall -Werror -fvisibility=hidden)

add_library(prometheus-c SHARED
    prometheus-c.c
    prometheus-c.h
)

target_link_libraries(prometheus-c pthread m)

# prometheus-c.h includes stopwatch.h, so the stopwatch include path must
# propagate (PUBLIC) to every downstream consumer of this library.
target_include_directories(prometheus-c PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/stopwatch
)

install(TARGETS prometheus-c DESTINATION lib)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(tests)
