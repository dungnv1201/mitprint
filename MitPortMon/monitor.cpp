#include "monitor.h"
#include "port.h"
#include "job.h"

// Forward declarations for MONITOR2 vtable
static BOOL WINAPI Mon_EnumPorts(HANDLE hMonitor, LPWSTR pName, DWORD Level,
                                  LPBYTE pPorts, DWORD cbBuf, LPDWORD pcbNeeded,
                                  LPDWORD pcReturned);
static BOOL WINAPI Mon_OpenPort(HANDLE hMonitor, LPWSTR pName, PHANDLE pHandle);
static BOOL WINAPI Mon_OpenPortEx(HANDLE hMonitor, HANDLE hMonitorPort,
                                   LPWSTR pPortName, LPWSTR pPrinterName,
                                   PHANDLE pHandle, struct _MONITOR2 FAR* pMonitor);
static BOOL WINAPI Mon_StartDocPort(HANDLE hPort, LPWSTR pPrinterName,
                                     DWORD JobId, DWORD Level, LPBYTE pDocInfo);
static BOOL WINAPI Mon_WritePort(HANDLE hPort, LPBYTE pBuffer,
                                  DWORD cbBuf, LPDWORD pcbWritten);
static BOOL WINAPI Mon_ReadPort(HANDLE hPort, LPBYTE pBuffer,
                                 DWORD cbBuffer, LPDWORD pcbRead);
static BOOL WINAPI Mon_EndDocPort(HANDLE hPort);
static BOOL WINAPI Mon_ClosePort(HANDLE hPort);
static BOOL WINAPI Mon_AddPort(HANDLE hMonitor, LPWSTR pName,
                                HWND hWnd, LPWSTR pMonitorName);
static BOOL WINAPI Mon_AddPortEx(HANDLE hMonitor, LPWSTR pName, DWORD Level,
                                  LPBYTE lpBuffer, LPWSTR lpMonitorName);
static BOOL WINAPI Mon_ConfigurePort(HANDLE hMonitor, LPWSTR pName,
                                      HWND hWnd, LPWSTR pPortName);
static BOOL WINAPI Mon_DeletePort(HANDLE hMonitor, LPWSTR pName,
                                   HWND hWnd, LPWSTR pPortName);
static BOOL WINAPI Mon_GetPrinterDataFromPort(HANDLE hPort, DWORD ControlID,
                                               LPWSTR pValueName, LPWSTR lpInBuffer,
                                               DWORD cbInBuffer, LPWSTR lpOutBuffer,
                                               DWORD cbOutBuffer, LPDWORD lpcbReturned);
static BOOL WINAPI Mon_SetPortTimeOuts(HANDLE hPort, LPCOMMTIMEOUTS lpCTO,
                                        DWORD reserved);
static BOOL WINAPI Mon_XcvOpenPort(HANDLE hMonitor, LPCWSTR pObject,
                                    ACCESS_MASK GrantedAccess, PHANDLE phXcv);
static DWORD WINAPI Mon_XcvDataPort(HANDLE hXcv, LPCWSTR pszDataName,
                                     PBYTE pInputData, DWORD cbInputData,
                                     PBYTE pOutputData, DWORD cbOutputData,
                                     PDWORD pcbOutputNeeded);
static BOOL WINAPI Mon_XcvClosePort(HANDLE hXcv);
static VOID WINAPI Mon_Shutdown(HANDLE hMonitor);

static MONITOR2 g_Monitor2 = {
    sizeof(MONITOR2),
    Mon_EnumPorts,
    Mon_OpenPort,
    Mon_OpenPortEx,
    Mon_StartDocPort,
    Mon_WritePort,
    Mon_ReadPort,
    Mon_EndDocPort,
    Mon_ClosePort,
    Mon_AddPort,
    Mon_AddPortEx,
    Mon_ConfigurePort,
    Mon_DeletePort,
    Mon_GetPrinterDataFromPort,
    Mon_SetPortTimeOuts,
    Mon_XcvOpenPort,
    Mon_XcvDataPort,
    Mon_XcvClosePort,
    Mon_Shutdown,
};

// ---------------------------------------------------------------
// Entry point called by spooler when monitor DLL is loaded
// ---------------------------------------------------------------
LPMONITOR2 WINAPI InitializePrintMonitor2(PMONITORINIT pMonitorInit, PHANDLE phMonitor)
{
    __try
    {
        if (!pMonitorInit || !phMonitor)
            return NULL;

        MitMonitorHandle* pMon = (MitMonitorHandle*)HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MitMonitorHandle));
        if (!pMon)
            return NULL;

        pMon->signature = 'MITM';
        pMon->hHeap     = GetProcessHeap();
        *phMonitor      = (HANDLE)pMon;
        return &g_Monitor2;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return NULL;
    }
}

// ---------------------------------------------------------------
// Port enumeration — expose "MitPort:" as the single port
// ---------------------------------------------------------------
static BOOL WINAPI Mon_EnumPorts(HANDLE hMonitor, LPWSTR pName, DWORD Level,
                                  LPBYTE pPorts, DWORD cbBuf, LPDWORD pcbNeeded,
                                  LPDWORD pcReturned)
{
    __try
    {
        const WCHAR kPortName[] = L"MitPort:";
        if (Level == 1)
        {
            DWORD needed = sizeof(PORT_INFO_1) + sizeof(kPortName);
            *pcbNeeded = needed;
            *pcReturned = 0;
            if (cbBuf < needed)
            {
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                return FALSE;
            }
            PORT_INFO_1* pi = (PORT_INFO_1*)pPorts;
            WCHAR* pName2   = (WCHAR*)(pPorts + sizeof(PORT_INFO_1));
            lstrcpyW(pName2, kPortName);
            pi->pName = pName2;
            *pcReturned = 1;
            return TRUE;
        }
        SetLastError(ERROR_INVALID_LEVEL);
        return FALSE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return FALSE; }
}

static BOOL WINAPI Mon_OpenPort(HANDLE hMonitor, LPWSTR pName, PHANDLE pHandle)
{
    return MitOpenPort(hMonitor, pName, pHandle);
}

static BOOL WINAPI Mon_OpenPortEx(HANDLE hMonitor, HANDLE hMonitorPort,
                                   LPWSTR pPortName, LPWSTR pPrinterName,
                                   PHANDLE pHandle, struct _MONITOR2 FAR* pMonitor)
{
    (void)hMonitorPort; (void)pPrinterName; (void)pMonitor;
    return MitOpenPort(hMonitor, pPortName, pHandle);
}

static BOOL WINAPI Mon_StartDocPort(HANDLE hPort, LPWSTR pPrinterName,
                                     DWORD JobId, DWORD Level, LPBYTE pDocInfo)
{
    return MitStartDocPort(hPort, pPrinterName, JobId, Level, pDocInfo);
}

static BOOL WINAPI Mon_WritePort(HANDLE hPort, LPBYTE pBuffer,
                                  DWORD cbBuf, LPDWORD pcbWritten)
{
    return MitWritePort(hPort, pBuffer, cbBuf, pcbWritten);
}

static BOOL WINAPI Mon_ReadPort(HANDLE hPort, LPBYTE pBuffer,
                                 DWORD cbBuffer, LPDWORD pcbRead)
{
    (void)hPort; (void)pBuffer; (void)cbBuffer;
    if (pcbRead) *pcbRead = 0;
    return FALSE; // not a bidirectional port
}

static BOOL WINAPI Mon_EndDocPort(HANDLE hPort)
{
    return MitEndDocPort(hPort);
}

static BOOL WINAPI Mon_ClosePort(HANDLE hPort)
{
    return MitClosePort(hPort);
}

static BOOL WINAPI Mon_AddPort(HANDLE hMonitor, LPWSTR pName,
                                HWND hWnd, LPWSTR pMonitorName)
{
    // Port is pre-configured; no interactive add needed
    (void)hMonitor; (void)pName; (void)hWnd; (void)pMonitorName;
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

static BOOL WINAPI Mon_AddPortEx(HANDLE hMonitor, LPWSTR pName, DWORD Level,
                                  LPBYTE lpBuffer, LPWSTR lpMonitorName)
{
    (void)hMonitor; (void)pName; (void)Level; (void)lpBuffer; (void)lpMonitorName;
    return TRUE;
}

static BOOL WINAPI Mon_ConfigurePort(HANDLE hMonitor, LPWSTR pName,
                                      HWND hWnd, LPWSTR pPortName)
{
    (void)hMonitor; (void)pName; (void)hWnd; (void)pPortName;
    return TRUE;
}

static BOOL WINAPI Mon_DeletePort(HANDLE hMonitor, LPWSTR pName,
                                   HWND hWnd, LPWSTR pPortName)
{
    (void)hMonitor; (void)pName; (void)hWnd; (void)pPortName;
    return TRUE;
}

static BOOL WINAPI Mon_GetPrinterDataFromPort(HANDLE hPort, DWORD ControlID,
                                               LPWSTR pValueName, LPWSTR lpInBuffer,
                                               DWORD cbInBuffer, LPWSTR lpOutBuffer,
                                               DWORD cbOutBuffer, LPDWORD lpcbReturned)
{
    (void)hPort; (void)ControlID; (void)pValueName;
    (void)lpInBuffer; (void)cbInBuffer; (void)lpOutBuffer; (void)cbOutBuffer;
    if (lpcbReturned) *lpcbReturned = 0;
    return FALSE;
}

static BOOL WINAPI Mon_SetPortTimeOuts(HANDLE hPort, LPCOMMTIMEOUTS lpCTO, DWORD reserved)
{
    (void)hPort; (void)lpCTO; (void)reserved;
    return TRUE;
}

static BOOL WINAPI Mon_XcvOpenPort(HANDLE hMonitor, LPCWSTR pObject,
                                    ACCESS_MASK GrantedAccess, PHANDLE phXcv)
{
    return MitXcvOpenPort(hMonitor, pObject, GrantedAccess, phXcv);
}

static DWORD WINAPI Mon_XcvDataPort(HANDLE hXcv, LPCWSTR pszDataName,
                                     PBYTE pInputData, DWORD cbInputData,
                                     PBYTE pOutputData, DWORD cbOutputData,
                                     PDWORD pcbOutputNeeded)
{
    return MitXcvDataPort(hXcv, pszDataName, pInputData, cbInputData,
                          pOutputData, cbOutputData, pcbOutputNeeded);
}

static BOOL WINAPI Mon_XcvClosePort(HANDLE hXcv)
{
    return MitXcvClosePort(hXcv);
}

static VOID WINAPI Mon_Shutdown(HANDLE hMonitor)
{
    __try
    {
        MitMonitorHandle* pMon = (MitMonitorHandle*)hMonitor;
        if (pMon && pMon->signature == 'MITM')
        {
            pMon->signature = 0;
            HeapFree(GetProcessHeap(), 0, pMon);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}
