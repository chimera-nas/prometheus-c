project(prometheus-c)

add_definitions(-O3 -Wall -Werror)

add_library(prometheus-c STATIC
    prometheus-c.c
    prometheus-c.h
)

target_link_libraries(prometheus-c pthread m)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(tests)