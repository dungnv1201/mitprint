#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winspool.h>
#include <winsplp.h>

// Monitor-level handle (one per monitor instance, created by spooler)
struct MitMonitorHandle
{
    DWORD    signature;   // 'MITM'
    HANDLE   hHeap;       // private heap for allocations inside spoolsv
};

#ifdef __cplusplus
extern "C" {
#endif

LPMONITOR2 WINAPI InitializePrintMonitor2(PMONITORINIT pMonitorInit, PHANDLE phMonitor);

#ifdef __cplusplus
}
#endif
