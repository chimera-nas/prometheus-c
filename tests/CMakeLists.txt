# SPDX-FileCopyrightText: 2025 Ben Jarvis
#
# SPDX-License-Identifier: LGPL-2.1-only

add_executable(counter counter.c)
add_executable(gauge gauge.c)
add_executable(histogram histogram.c)
add_executable(time time.c)

target_link_libraries(counter prometheus-c)
target_link_libraries(gauge prometheus-c)
target_link_libraries(histogram prometheus-c)
target_link_libraries(time prometheus-c)

add_test(NAME prometheus-c/counter COMMAND counter)
add_test(NAME prometheus-c/gauge COMMAND gauge)
add_test(NAME prometheus-c/histogram COMMAND histogram)
add_test(NAME prometheus-c/time COMMAND time)

add_executable(concurrent concurrent.c)
target_link_libraries(concurrent prometheus-c Threads::Threads)
if(MSVC)
    target_compile_options(concurrent PRIVATE /UNDEBUG)
else()
    target_compile_options(concurrent PRIVATE -UNDEBUG)
endif()
add_test(NAME prometheus-c/concurrent COMMAND concurrent)

# Windows loads a dependent DLL beside the executable; Unix uses CMake's rpath.
if(WIN32)
    foreach(test counter gauge histogram time concurrent)
        add_custom_command(TARGET ${test} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:prometheus-c> $<TARGET_FILE_DIR:${test}>
            VERBATIM)
    endforeach()
endif()
