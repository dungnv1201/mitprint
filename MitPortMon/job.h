#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winspool.h>

BOOL WINAPI MitStartDocPort(HANDLE hPort, LPWSTR pPrinterName,
                             DWORD JobId, DWORD Level, LPBYTE pDocInfo);
BOOL WINAPI MitWritePort(HANDLE hPort, LPBYTE pBuffer,
                          DWORD cbBuf, LPDWORD pcbWritten);
BOOL WINAPI MitEndDocPort(HANDLE hPort);
