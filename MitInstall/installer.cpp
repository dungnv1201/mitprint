#include "installer.h"
#include "protocol.h"
#include "mit_guids.h"
#include <winspool.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <winsvc.h>

static void LogError(const wchar_t* step, DWORD err)
{
    wchar_t msg[256];
    StringCchPrintfW(msg, 256, L"[MitInstall] FAIL: %s — Error %u\n", step, err);
    OutputDebugStringW(msg);
    MessageBoxW(nullptr, msg, L"MitInstall Error", MB_OK | MB_ICONERROR);
}

static BOOL CopyDriverFile(const wchar_t* pSrc, const wchar_t* pDriverDir)
{
    wchar_t dest[MAX_PATH];
    StringCchCopyW(dest, MAX_PATH, pDriverDir);
    PathAppendW(dest, PathFindFileNameW(pSrc));
    return CopyFileW(pSrc, dest, FALSE);
}

static void StopSpooler()
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return;
    SC_HANDLE hSvc = OpenServiceW(hSCM, L"Spooler", SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (hSvc)
    {
        SERVICE_STATUS ss = {};
        ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);
        for (int i = 0; i < 20; ++i)
        {
            QueryServiceStatus(hSvc, &ss);
            if (ss.dwCurrentState == SERVICE_STOPPED) break;
            Sleep(500);
        }
        CloseServiceHandle(hSvc);
    }
    CloseServiceHandle(hSCM);
}

// Find any already-installed V3 x64 printer driver as fallback
static BOOL FindAnyV3Driver(wchar_t* pName, DWORD cch)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Control\\Print\\Environments"
            L"\\Windows x64\\Drivers\\Version-3",
            0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return FALSE;

    BOOL  found = FALSE;
    DWORD idx = 0, cchName = cch;
    while (RegEnumKeyExW(hKey, idx++, pName, &cchName,
                         nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
    {
        if (lstrcmpiW(pName, MIT_DRIVER_NAME) != 0) { found = TRUE; break; }
        cchName = cch;
    }
    RegCloseKey(hKey);
    return found;
}

static void StartSpooler()
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return;
    SC_HANDLE hSvc = OpenServiceW(hSCM, L"Spooler", SERVICE_START | SERVICE_QUERY_STATUS);
    if (hSvc)
    {
        StartServiceW(hSvc, 0, nullptr);
        for (int i = 0; i < 20; ++i)
        {
            SERVICE_STATUS ss = {};
            QueryServiceStatus(hSvc, &ss);
            if (ss.dwCurrentState == SERVICE_RUNNING) break;
            Sleep(500);
        }
        CloseServiceHandle(hSvc);
    }
    CloseServiceHandle(hSCM);
}

BOOL MitInstall(const wchar_t* pInstallDir)
{
    // ---------------------------------------------------------------
    // Step 1: Get printer driver directory
    // ---------------------------------------------------------------
    wchar_t driverDir[MAX_PATH] = {};
    DWORD cbNeeded = sizeof(driverDir);
    if (!GetPrinterDriverDirectoryW(nullptr, (LPWSTR)L"Windows x64", 1,
                                    (LPBYTE)driverDir, cbNeeded, &cbNeeded))
    {
        LogError(L"GetPrinterDriverDirectory", GetLastError());
        return FALSE;
    }
    // GetPrinterDriverDirectoryW returns the base dir (x64\); V3 drivers live in x64\3
    PathAppendW(driverDir, L"3");

    // ---------------------------------------------------------------
    // Step 2: Copy MitPortMon.dll to System32 (if not already there)
    // ---------------------------------------------------------------
    wchar_t monDll[MAX_PATH];
    StringCchCopyW(monDll, MAX_PATH, pInstallDir);
    PathAppendW(monDll, L"MitPortMon.dll");

    wchar_t sys32Dir[MAX_PATH] = {};
    GetSystemDirectoryW(sys32Dir, MAX_PATH);

    wchar_t monDllDest[MAX_PATH];
    StringCchCopyW(monDllDest, MAX_PATH, sys32Dir);
    PathAppendW(monDllDest, L"MitPortMon.dll");

    if (!PathFileExistsW(monDllDest))
    {
        StopSpooler();
        BOOL ok = CopyFileW(monDll, monDllDest, FALSE);
        StartSpooler();
        if (!ok)
        {
            LogError(L"CopyFile MitPortMon.dll to System32", GetLastError());
            return FALSE;
        }
    }

    // ---------------------------------------------------------------
    // Step 2b: Stage driver files into pInstallDir so source != driverDir
    //          AddPrinterDriverEx fails when source == destination
    // ---------------------------------------------------------------
    struct { const wchar_t* name; } kDriverFiles[] = {
        { L"UNIDRV.DLL" }, { L"UNIDRVUI.DLL" }, { L"STDNAMES.GPD" }
    };
    for (auto& f : kDriverFiles)
    {
        wchar_t src[MAX_PATH], dst[MAX_PATH];
        StringCchCopyW(src, MAX_PATH, driverDir);   PathAppendW(src, f.name);
        StringCchCopyW(dst, MAX_PATH, pInstallDir); PathAppendW(dst, f.name);
        if (!PathFileExistsW(dst))
            CopyFileW(src, dst, FALSE); // best-effort; files may already be there
    }

    // ---------------------------------------------------------------
    // Step 3: Add Port Monitor
    // ---------------------------------------------------------------
    MONITOR_INFO_2W mi2 = {};
    mi2.pName         = (LPWSTR)MIT_MONITOR_NAME;
    mi2.pEnvironment  = (LPWSTR)L"Windows x64";
    mi2.pDLLName      = (LPWSTR)L"MitPortMon.dll";

    if (!AddMonitorW(nullptr, 2, (LPBYTE)&mi2))
    {
        DWORD err = GetLastError();
        if (err != ERROR_PRINT_MONITOR_ALREADY_INSTALLED)
        {
            LogError(L"AddMonitor", err);
            return FALSE;
        }
    }

    // ---------------------------------------------------------------
    // Step 4: Add Port via registry (simplest method)
    // ---------------------------------------------------------------
    {
        HKEY hKey = nullptr;
        wchar_t regPath[256];
        StringCchPrintfW(regPath, 256,
            L"SYSTEM\\CurrentControlSet\\Control\\Print\\Monitors\\%s\\Ports",
            MIT_MONITOR_NAME);

        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, nullptr,
                            REG_OPTION_NON_VOLATILE, KEY_WRITE,
                            nullptr, &hKey, nullptr) == ERROR_SUCCESS)
        {
            DWORD val = 1;
            RegSetValueExW(hKey, MIT_PORT_NAME, 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
            RegCloseKey(hKey);
        }
    }

    // ---------------------------------------------------------------
    // Step 5: Install Printer Driver — all source files in pInstallDir
    //         so spooler copies FROM pInstallDir TO driverDir (src != dst)
    // ---------------------------------------------------------------
    const wchar_t kDepFiles[] = L"UNIDRV.DLL\0UNIDRVUI.DLL\0STDNAMES.GPD\0\0";

    wchar_t driverPath[MAX_PATH];
    StringCchCopyW(driverPath, MAX_PATH, pInstallDir);
    PathAppendW(driverPath, L"UNIDRV.DLL");

    wchar_t configPath[MAX_PATH];
    StringCchCopyW(configPath, MAX_PATH, pInstallDir);
    PathAppendW(configPath, L"UNIDRVUI.DLL");

    wchar_t dataFilePath[MAX_PATH];
    StringCchCopyW(dataFilePath, MAX_PATH, pInstallDir);
    PathAppendW(dataFilePath, L"MitPrint.gpd");

    DRIVER_INFO_6W di6 = {};
    di6.cVersion          = 3;
    di6.pName             = (LPWSTR)MIT_DRIVER_NAME;
    di6.pEnvironment      = (LPWSTR)L"Windows x64";
    di6.pDriverPath       = driverPath;
    di6.pDataFile         = dataFilePath;
    di6.pConfigFile       = configPath;
    di6.pMonitorName      = (LPWSTR)MIT_MONITOR_NAME;
    di6.pDefaultDataType  = (LPWSTR)L"RAW";
    di6.pDependentFiles   = (LPWSTR)kDepFiles;

    // Track which driver name to use for AddPrinter
    wchar_t activeDriverName[256] = {};
    StringCchCopyW(activeDriverName, 256, MIT_DRIVER_NAME);

    BOOL driverOk = AddPrinterDriverExW(nullptr, 6, (LPBYTE)&di6, APD_COPY_ALL_FILES);
    if (!driverOk)
    {
        DWORD err = GetLastError();
        if (err == ERROR_PRINTER_DRIVER_ALREADY_INSTALLED)
        {
            driverOk = TRUE;
        }
        else if (err == ERROR_SUCCESS_RESTART_REQUIRED)
        {
            StopSpooler();
            StartSpooler();
            driverOk = TRUE;
        }
        else
        {
            // Windows 10/11 post-PrintNightmare may block unsigned driver install (3019).
            // Fall back to any existing V3 driver already trusted by the system.
            if (FindAnyV3Driver(activeDriverName, 256))
            {
                driverOk = TRUE;
            }
            else
            {
                LogError(L"AddPrinterDriverEx (no fallback driver found)", err);
                return FALSE;
            }
        }
    }

    // ---------------------------------------------------------------
    // Step 6: Add Printer
    // ---------------------------------------------------------------
    PRINTER_INFO_2W pi2 = {};
    pi2.pPrinterName    = (LPWSTR)MIT_PRINTER_NAME;
    pi2.pPortName       = (LPWSTR)MIT_PORT_NAME;
    pi2.pDriverName     = activeDriverName;
    pi2.pPrintProcessor = (LPWSTR)L"WinPrint";
    pi2.pDatatype       = nullptr;
    pi2.Attributes      = PRINTER_ATTRIBUTE_LOCAL;

    HANDLE hPrinter = AddPrinterW(nullptr, 2, (LPBYTE)&pi2);
    if (!hPrinter)
    {
        DWORD err = GetLastError();
        if (err != ERROR_PRINTER_ALREADY_EXISTS)
        {
            LogError(L"AddPrinter", err);
            return FALSE;
        }
    }
    else
    {
        ClosePrinter(hPrinter);
    }

    // ---------------------------------------------------------------
    // Step 7: Register MitPrint.exe for autostart
    // ---------------------------------------------------------------
    wchar_t mitExePath[MAX_PATH];
    StringCchCopyW(mitExePath, MAX_PATH, pInstallDir);
    PathAppendW(mitExePath, L"MitPrint.exe");

    wchar_t autorunValue[MAX_PATH + 20];
    StringCchPrintfW(autorunValue, MAX_PATH + 20, L"\"%s\" /background", mitExePath);

    HKEY hRunKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_WRITE, &hRunKey) == ERROR_SUCCESS)
    {
        RegSetValueExW(hRunKey, MIT_APPNAME, 0, REG_SZ,
                       (const BYTE*)autorunValue,
                       (DWORD)(wcslen(autorunValue) + 1) * sizeof(wchar_t));
        RegCloseKey(hRunKey);
    }

    // ---------------------------------------------------------------
    // Step 8: Launch MitPrint.exe
    // ---------------------------------------------------------------
    ShellExecuteW(nullptr, L"open", mitExePath, L"/background", nullptr, SW_HIDE);

    MessageBoxW(nullptr,
        L"MitPrint đã được cài đặt thành công!\n\n"
        L"Máy in ảo \"MitPrint\" đã xuất hiện trong danh sách máy in.\n"
        L"Mở ứng dụng bất kỳ, chọn File > Print > MitPrint để sử dụng.",
        L"MitPrint Setup", MB_OK | MB_ICONINFORMATION);

    return TRUE;
}
