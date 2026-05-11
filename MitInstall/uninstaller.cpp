#include "uninstaller.h"
#include "protocol.h"
#include "mit_guids.h"
#include <strsafe.h>

BOOL MitUninstall()
{
    // ---------------------------------------------------------------
    // Step 1: Kill MitPrint.exe gracefully
    // ---------------------------------------------------------------
    {
        HWND hWnd = FindWindowW(L"MitPrintMsgWnd", nullptr);
        if (hWnd)
        {
            PostMessageW(hWnd, WM_CLOSE, 0, 0);
            Sleep(1500);
        }
    }

    // ---------------------------------------------------------------
    // Step 2: Delete the virtual printer
    // ---------------------------------------------------------------
    HANDLE hPrinter = nullptr;
    if (OpenPrinterW((LPWSTR)MIT_PRINTER_NAME, &hPrinter, nullptr))
    {
        SetPrinterW(hPrinter, 0, nullptr, PRINTER_CONTROL_PURGE);
        ClosePrinter(hPrinter);
        hPrinter = nullptr;

        // Re-open to delete
        if (OpenPrinterW((LPWSTR)MIT_PRINTER_NAME, &hPrinter, nullptr))
        {
            DeletePrinter(hPrinter);
            ClosePrinter(hPrinter);
        }
    }

    // ---------------------------------------------------------------
    // Step 3: Delete printer driver
    // ---------------------------------------------------------------
    DeletePrinterDriverExW(nullptr, (LPWSTR)L"Windows x64",
                           (LPWSTR)MIT_DRIVER_NAME,
                           DPD_DELETE_UNUSED_FILES, 0);

    // ---------------------------------------------------------------
    // Step 4: Delete port monitor
    // ---------------------------------------------------------------
    DeleteMonitorW(nullptr, (LPWSTR)L"Windows x64", (LPWSTR)MIT_MONITOR_NAME);

    // ---------------------------------------------------------------
    // Step 5: Remove registry port entry
    // ---------------------------------------------------------------
    {
        wchar_t regPath[256];
        StringCchPrintfW(regPath, 256,
            L"SYSTEM\\CurrentControlSet\\Control\\Print\\Monitors\\%s\\Ports",
            MIT_MONITOR_NAME);
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
        {
            RegDeleteValueW(hKey, MIT_PORT_NAME);
            RegCloseKey(hKey);
        }
    }

    // ---------------------------------------------------------------
    // Step 6: Remove autostart entry
    // ---------------------------------------------------------------
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
        {
            RegDeleteValueW(hKey, MIT_APPNAME);
            RegCloseKey(hKey);
        }
    }

    MessageBoxW(nullptr, L"MitPrint đã được gỡ cài đặt thành công.",
                L"MitPrint Uninstall", MB_OK | MB_ICONINFORMATION);
    return TRUE;
}
