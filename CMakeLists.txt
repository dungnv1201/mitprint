cmake_minimum_required(VERSION 3.25)
project(MitPrint VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "MitPrint is Windows-only")
endif()

set(MIT_SHARED_DIR "${CMAKE_SOURCE_DIR}/shared")

add_compile_definitions(
    UNICODE
    _UNICODE
    STRICT
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
    NOMINMAX
)

add_subdirectory(shared)
add_subdirectory(MitPortMon)
add_subdirectory(MitPrint)
add_subdirectory(MitInstall)
