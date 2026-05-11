#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winspool.h>

// Per-port context (one per OpenPort call)
struct MitPortContext
{
    DWORD    signature;       // 'MITP'
    WCHAR    portName[64];
    HANDLE   hPipe;           // named pipe connection to MitPrint.exe
    DWORD    currentJobId;
    DWORD    sequenceNum;
    BOOL     jobActive;
    CRITICAL_SECTION cs;
};

// XCV context (for XcvOpenPort / XcvDataPort)
struct MitXcvContext
{
    DWORD        signature;   // 'MITX'
    ACCESS_MASK  access;
    WCHAR        portName[64];
};

BOOL WINAPI MitOpenPort(HANDLE hMonitor, LPWSTR pName, PHANDLE pHandle);
BOOL WINAPI MitClosePort(HANDLE hPort);
BOOL WINAPI MitXcvOpenPort(HANDLE hMonitor, LPCWSTR pObject,
                            ACCESS_MASK GrantedAccess, PHANDLE phXcv);
DWORD WINAPI MitXcvDataPort(HANDLE hXcv, LPCWSTR pszDataName,
                             PBYTE pInputData, DWORD cbInputData,
                             PBYTE pOutputData, DWORD cbOutputData,
                             PDWORD pcbOutputNeeded);
BOOL WINAPI MitXcvClosePort(HANDLE hXcv);
