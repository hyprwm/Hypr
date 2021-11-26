cmake_minimum_required(VERSION 3.4)
project(Hypr 
    VERSION 0.1
    DESCRIPTION "A Modern OOP C++ Window Manager"
)

add_compile_options(-std=c++17)
add_compile_options(-Wall -Wextra)
find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET glib-2.0 harfbuzz cairo gtkmm-3.0 xcb-randr)

file(GLOB_RECURSE SRCFILES "src/*.cpp")

add_executable(Hypr ${SRCFILES})

target_link_libraries(Hypr rt)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

target_link_libraries(Hypr PkgConfig::deps)

target_link_libraries(Hypr
        xcb
        xcb-ewmh
        xcb-icccm
        xcb-keysyms
        xcb-randr
        xcb-xinerama
        xcb-cursor
        ${CMAKE_THREAD_LIBS_INIT}
)