cmake_minimum_required(VERSION 3.28)
project(ongearm VERSION 0.1)
set(CMAKE_CXX_STANDARD 23)
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall)
endif()

include_directories(
    include
    external/include/boost
    external/include/tomlplusplus)

add_executable(wdmltd
    src/wdmltd/phantom.cpp
    src/wdmltd/main.cpp)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/config.toml
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/src/wdmltd/config.toml ${CMAKE_BINARY_DIR})
add_custom_target(wdmltd_config ALL DEPENDS ${CMAKE_BINARY_DIR}/config.toml)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    enable_testing()
    add_executable(${CMAKE_PROJECT_NAME}_test0
        src/test/t0.cpp)
    add_test(NAME test0 COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}_test0)
endif()
