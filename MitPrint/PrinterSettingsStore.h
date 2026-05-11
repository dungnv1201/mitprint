#pragma once
#include <string>
#include <windows.h>

struct CPrinterSettings
{
    std::wstring targetPrinter;
    DWORD        paperSize    = DMPAPER_A4;
    short        copies       = 1;
    short        duplex       = DMDUP_SIMPLEX;  // DMDUP_*
    short        orientation  = DMORIENT_PORTRAIT;
};

class CPrinterSettingsStore
{
public:
    static void Load(CPrinterSettings& s);
    static void Save(const CPrinterSettings& s);

private:
    static HKEY OpenSettingsKey(BOOL bWrite);
};
