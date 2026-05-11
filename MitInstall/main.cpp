#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include "installer.h"
#include "uninstaller.h"

// Usage:
//   MitInstall.exe              — install
//   MitInstall.exe /uninstall   — uninstall

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR lpCmdLine, int)
{
    if (lpCmdLine && lstrcmpiW(lpCmdLine, L"/uninstall") == 0)
    {
        MitUninstall();
        return 0;
    }

    // Determine install directory (same dir as MitInstall.exe)
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);

    MitInstall(exePath);
    return 0;
}
