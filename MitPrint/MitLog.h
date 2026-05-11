#pragma once
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

// Writes timestamped ASCII lines to %TEMP%\MitPrint.log
// Safe to call from any thread.
inline void MitLog(const char* fmt, ...)
{
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    _vsnprintf_s(msg, sizeof(msg), _TRUNCATE, fmt, args);
    va_end(args);

    OutputDebugStringA("[MitPrint] ");
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");

    wchar_t logPath[MAX_PATH];
    GetTempPathW(MAX_PATH, logPath);
    wcscat_s(logPath, L"MitPrint.log");

    HANDLE hFile = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    char line[1200];
    _snprintf_s(line, sizeof(line), _TRUNCATE,
                "[%02d:%02d:%02d.%03d] %s\r\n",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);

    DWORD written = 0;
    WriteFile(hFile, line, (DWORD)strlen(line), &written, nullptr);
    CloseHandle(hFile);
}
