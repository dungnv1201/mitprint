#pragma once
#include <windows.h>

// Registry paths
constexpr wchar_t MIT_REG_SETTINGS[]  = L"Software\\MitPrint\\Settings";
constexpr wchar_t MIT_REG_AUTORUN[]   = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t MIT_APPNAME[]        = L"MitPrint";
constexpr wchar_t MIT_WINDOW_CLASS[]   = L"MitPrintMsgWnd";

// Tray icon ID
constexpr UINT MIT_TRAY_ICON_ID       = 1;
constexpr UINT WM_MIT_TRAY            = WM_USER + 100;
constexpr UINT WM_MIT_JOB_ADDED       = WM_USER + 101;
constexpr UINT WM_MIT_JOB_STATUS      = WM_USER + 102;
constexpr UINT WM_MIT_SHOW_QUEUE      = WM_USER + 103;
constexpr UINT WM_MIT_SHOW_SETTINGS   = WM_USER + 104;

