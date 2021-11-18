cmake_minimum_required(VERSION 3.4)
project(Hypr 
    VERSION 0.1
    DESCRIPTION "A Modern OOP C++ Window Manager"
)

add_compile_options(-std=c++17)

file(GLOB_RECURSE SRCFILES "src/*.cpp")

add_executable(Hypr ${SRCFILES})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

target_link_libraries(Hypr
        xcb
        xcb-ewmh
        xcb-icccm
        xcb-keysyms
        xcb-randr
        xcb-xinerama
)