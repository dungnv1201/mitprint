cmake_minimum_required(VERSION 3.25)
project(MitPrintApp)

# Static MFC (1 = static, 2 = shared DLL)
set(CMAKE_MFC_FLAG 1)

add_executable(MitPrint WIN32
    main.cpp
    MitPrintApp.cpp
    TrayIcon.cpp
    PrintQueueDlg.cpp
    SettingsDlg.cpp
    PrinterSettingsStore.cpp
    PipeServer.cpp
    JobProcessor.cpp
    EmfRenderer.cpp
    MitPrint.rc
)

target_include_directories(MitPrint PRIVATE
    "${MIT_SHARED_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(MitPrint PRIVATE
    winspool
    gdi32
    user32
    shell32
    advapi32
    comctl32
    comdlg32
    ole32
    shlwapi
)

target_compile_options(MitPrint PRIVATE
    $<$<CONFIG:Debug>:/MTd>
    $<$<CONFIG:Release>:/MT>
    $<$<CONFIG:RelWithDebInfo>:/MT>
)

set_target_properties(MitPrint PROPERTIES OUTPUT_NAME "MitPrint")
