# SPDX-FileCopyrightText: 2025 Ben Jarvis
#
# SPDX-License-Identifier: LGPL-2.1-only

add_executable(counter counter.c)
add_executable(gauge gauge.c)
add_executable(histogram histogram.c)

target_link_libraries(counter prometheus-c)
target_link_libraries(gauge prometheus-c)
target_link_libraries(histogram prometheus-c)

add_test(NAME prometheus-c/counter COMMAND counter)
add_test(NAME prometheus-c/gauge COMMAND gauge)
add_test(NAME prometheus-c/histogram COMMAND histogram)
