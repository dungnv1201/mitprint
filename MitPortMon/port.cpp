#include "port.h"
#include "pipe_client.h"
#include "protocol.h"

BOOL WINAPI MitOpenPort(HANDLE hMonitor, LPWSTR pName, PHANDLE pHandle)
{
    __try
    {
        if (!pName || !pHandle) return FALSE;

        MitPortContext* ctx = (MitPortContext*)HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MitPortContext));
        if (!ctx) return FALSE;

        ctx->signature    = 'MITP';
        ctx->hPipe        = INVALID_HANDLE_VALUE;
        ctx->currentJobId = 0;
        ctx->sequenceNum  = 0;
        ctx->jobActive    = FALSE;
        InitializeCriticalSection(&ctx->cs);

        lstrcpynW(ctx->portName, pName,
                  sizeof(ctx->portName) / sizeof(ctx->portName[0]) - 1);

        // Connect to MitPrint.exe named pipe
        ctx->hPipe = MitPipeConnect();
        // Not a fatal error if app is not running; job will fail at StartDocPort
        (void)hMonitor;

        *pHandle = (HANDLE)ctx;
        return TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return FALSE; }
}

BOOL WINAPI MitClosePort(HANDLE hPort)
{
    __try
    {
        MitPortContext* ctx = (MitPortContext*)hPort;
        if (!ctx || ctx->signature != 'MITP') return FALSE;

        if (ctx->hPipe != INVALID_HANDLE_VALUE)
            CloseHandle(ctx->hPipe);

        DeleteCriticalSection(&ctx->cs);
        ctx->signature = 0;
        HeapFree(GetProcessHeap(), 0, ctx);
        return TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return FALSE; }
}

// ---------------------------------------------------------------
// XCV — used by installer to add/verify the port
// ---------------------------------------------------------------
BOOL WINAPI MitXcvOpenPort(HANDLE hMonitor, LPCWSTR pObject,
                            ACCESS_MASK GrantedAccess, PHANDLE phXcv)
{
    __try
    {
        (void)hMonitor;
        MitXcvContext* xcv = (MitXcvContext*)HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MitXcvContext));
        if (!xcv) return FALSE;

        xcv->signature = 'MITX';
        xcv->access    = GrantedAccess;
        if (pObject)
            lstrcpynW(xcv->portName, pObject,
                      sizeof(xcv->portName) / sizeof(xcv->portName[0]) - 1);

        *phXcv = (HANDLE)xcv;
        return TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return FALSE; }
}

DWORD WINAPI MitXcvDataPort(HANDLE hXcv, LPCWSTR pszDataName,
                             PBYTE pInputData, DWORD cbInputData,
                             PBYTE pOutputData, DWORD cbOutputData,
                             PDWORD pcbOutputNeeded)
{
    __try
    {
        (void)hXcv; (void)pInputData; (void)cbInputData;
        (void)pOutputData; (void)cbOutputData;
        if (pcbOutputNeeded) *pcbOutputNeeded = 0;

        if (lstrcmpiW(pszDataName, L"GetTransmissionRetryTimeout") == 0)
        {
            if (pcbOutputNeeded) *pcbOutputNeeded = sizeof(DWORD);
            if (cbOutputData >= sizeof(DWORD) && pOutputData)
                *(DWORD*)pOutputData = 90;
            return ERROR_SUCCESS;
        }
        return ERROR_INVALID_PARAMETER;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return ERROR_EXCEPTION_IN_SERVICE; }
}

BOOL WINAPI MitXcvClosePort(HANDLE hXcv)
{
    __try
    {
        MitXcvContext* xcv = (MitXcvContext*)hXcv;
        if (!xcv || xcv->signature != 'MITX') return FALSE;
        xcv->signature = 0;
        HeapFree(GetProcessHeap(), 0, xcv);
        return TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return FALSE; }
}
