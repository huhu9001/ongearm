cmake_minimum_required(VERSION 3.28)
project(ongearm VERSION 0.2)
set(CMAKE_CXX_STANDARD 23)
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall)
endif()

add_library(uring SHARED IMPORTED)
set_property(TARGET uring PROPERTY
    IMPORTED_IMPLIB ${CMAKE_SOURCE_DIR}/external/lib/uring/liburing.so)
add_compile_definitions(PUBLIC
    BOOST_ASIO_HAS_IO_URING
    TOML_EXCEPTIONS=0)

include_directories(
    include
    external/include/boost
    external/include/tomlplusplus
    external/include/uring)

add_executable(wdmltd
    src/wdmltd/config.cpp
    src/wdmltd/ctrl.cpp
    src/wdmltd/phantom.cpp
    src/wdmltd/ptsvr.cpp
    src/wdmltd/main.cpp
    src/wdmltd/main_daemon.cpp
    src/wdmltd/main_ctl.cpp)
target_link_libraries(wdmltd uring)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/config.toml
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/src/wdmltd/config.toml ${CMAKE_BINARY_DIR})
add_custom_target(wdmltd_config ALL DEPENDS ${CMAKE_BINARY_DIR}/config.toml)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    include_directories(src/test)
    enable_testing()
    add_executable(t_parser
        src/test/parser.cpp)
    add_executable(t_dpad
        src/wdmltd/test.dpad.cpp
        src/wdmltd/ctrl.cpp)
    add_test(NAME parser COMMAND ${CMAKE_BINARY_DIR}/t_parser)
    add_test(NAME dpad COMMAND ${CMAKE_BINARY_DIR}/t_dpad)
endif()
