cmake_minimum_required(VERSION 3.25)
project(MitInstall)

add_executable(MitInstall WIN32
    main.cpp
    installer.cpp
    uninstaller.cpp
)

target_include_directories(MitInstall PRIVATE
    "${MIT_SHARED_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(MitInstall PRIVATE
    winspool
    advapi32
    shell32
    user32
    shlwapi
)

target_compile_options(MitInstall PRIVATE
    $<$<CONFIG:Debug>:/MTd>
    $<$<CONFIG:Release>:/MT>
    $<$<CONFIG:RelWithDebInfo>:/MT>
)

# UAC manifest: requireAdministrator
set_target_properties(MitInstall PROPERTIES
    LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\""
)

set_target_properties(MitInstall PROPERTIES OUTPUT_NAME "MitInstall")
