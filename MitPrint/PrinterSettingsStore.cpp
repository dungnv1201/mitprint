#include <afxwin.h>
#include "PrinterSettingsStore.h"
#include "mit_guids.h"

static void RegReadString(HKEY hKey, const wchar_t* name, std::wstring& out)
{
    wchar_t buf[512] = {};
    DWORD   sz  = sizeof(buf);
    DWORD   type = REG_SZ;
    if (RegQueryValueExW(hKey, name, nullptr, &type, (LPBYTE)buf, &sz) == ERROR_SUCCESS)
        out = buf;
}

static void RegReadDword(HKEY hKey, const wchar_t* name, DWORD& out)
{
    DWORD sz = sizeof(DWORD), type = REG_DWORD;
    RegQueryValueExW(hKey, name, nullptr, &type, (LPBYTE)&out, &sz);
}

HKEY CPrinterSettingsStore::OpenSettingsKey(BOOL bWrite)
{
    HKEY hKey = nullptr;
    DWORD disp = 0;
    RegCreateKeyExW(HKEY_CURRENT_USER, MIT_REG_SETTINGS, 0, nullptr,
                    REG_OPTION_NON_VOLATILE,
                    bWrite ? KEY_WRITE : KEY_READ,
                    nullptr, &hKey, &disp);
    return hKey;
}

void CPrinterSettingsStore::Load(CPrinterSettings& s)
{
    HKEY hKey = OpenSettingsKey(FALSE);
    if (!hKey) return;

    RegReadString(hKey, L"TargetPrinter", s.targetPrinter);

    DWORD v = 0;
    RegReadDword(hKey, L"PaperSize",   v); if (v) s.paperSize   = v;
    RegReadDword(hKey, L"Copies",      v); if (v) s.copies       = (short)v;
    RegReadDword(hKey, L"Duplex",      v); if (v) s.duplex       = (short)v;
    RegReadDword(hKey, L"Orientation", v); if (v) s.orientation  = (short)v;

    RegCloseKey(hKey);
}

void CPrinterSettingsStore::Save(const CPrinterSettings& s)
{
    HKEY hKey = OpenSettingsKey(TRUE);
    if (!hKey) return;

    RegSetValueExW(hKey, L"TargetPrinter", 0, REG_SZ,
                   (const BYTE*)s.targetPrinter.c_str(),
                   (DWORD)(s.targetPrinter.size() + 1) * sizeof(wchar_t));

    DWORD v = s.paperSize;
    RegSetValueExW(hKey, L"PaperSize",   0, REG_DWORD, (LPBYTE)&v, sizeof(v));
    v = s.copies;
    RegSetValueExW(hKey, L"Copies",      0, REG_DWORD, (LPBYTE)&v, sizeof(v));
    v = s.duplex;
    RegSetValueExW(hKey, L"Duplex",      0, REG_DWORD, (LPBYTE)&v, sizeof(v));
    v = s.orientation;
    RegSetValueExW(hKey, L"Orientation", 0, REG_DWORD, (LPBYTE)&v, sizeof(v));

    RegCloseKey(hKey);
}
