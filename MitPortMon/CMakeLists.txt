cmake_minimum_required(VERSION 3.25)
project(MitPortMon)

add_library(MitPortMon SHARED
    dllmain.cpp
    monitor.cpp
    port.cpp
    job.cpp
    pipe_client.cpp
    MitPortMon.def
)

target_include_directories(MitPortMon PRIVATE
    "${MIT_SHARED_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(MitPortMon PRIVATE
    winspool
    kernel32
    user32
    advapi32
)

# Static CRT to avoid version conflicts inside spoolsv.exe
target_compile_options(MitPortMon PRIVATE
    $<$<CONFIG:Debug>:/MTd>
    $<$<CONFIG:Release>:/MT>
    $<$<CONFIG:RelWithDebInfo>:/MT>
)

set_target_properties(MitPortMon PROPERTIES OUTPUT_NAME "MitPortMon")

# Stage DLL for installer
add_custom_command(TARGET MitPortMon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/staging"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:MitPortMon>
        "${CMAKE_BINARY_DIR}/staging/MitPortMon.dll"
)
