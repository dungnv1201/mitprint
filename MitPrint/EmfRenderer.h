#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winspool.h>
#include <vector>
#include "PrinterSettingsStore.h"

// EMF spool record types — MS-EMFSPOOL spec
enum EmfSpoolRecordType : DWORD
{
    EMRI_METAFILE        = 1,   // EMF metafile (some drivers)
    EMRI_MIRRORMETAFILE  = 2,
    EMRI_HEADER          = 3,
    EMRI_DEVMODE         = 4,
    EMRI_FONT_DEF_IDEF   = 5,
    EMRI_DESIGNVECTOR    = 6,
    EMRI_SUBSET_FONT     = 7,
    EMRI_DELTA_FONT      = 8,
    EMRI_TYPE1_FONT      = 9,
    EMRI_PRESTARTPAGE    = 10,
    EMRI_DRAWORDER       = 11,
    EMRI_METAFILE_DATA   = 12,  // EMF page data (main page record)
    EMRI_METAFILE_EXT    = 13,  // extended EMF page data
    EMRI_BW_METAFILE_EXT = 14,
    EMRI_EOF             = 15,
};

#pragma pack(push, 1)
struct EmfSpoolFileHeader
{
    DWORD  dwVersion;     // 0x00010000
    DWORD  cjSize;        // total header size
    DWORD  dpszDocName;   // offset to doc name (wide string)
    DWORD  dpszOutput;    // offset to output port (wide string)
};

struct EmfSpoolRecord
{
    DWORD  ulID;    // EmfSpoolRecordType
    DWORD  cjSize;  // total record size including this header (8 bytes + data)
};
#pragma pack(pop)

class CEmfRenderer
{
public:
    // Save print job as PDF using "Microsoft Print To PDF" with output path
    BOOL RenderToPdf(const wchar_t* pDocName,
                     const BYTE*    pSpoolData,
                     DWORD          cbSpoolData,
                     const wchar_t* pOutputPath);

    BOOL RenderToRealPrinter(const wchar_t* pTargetPrinter,
                              const wchar_t* pDocName,
                              const BYTE*    pSpoolData,
                              DWORD          cbSpoolData,
                              const CPrinterSettings& settings);

private:
    struct PageData
    {
        const BYTE* pEmfData = nullptr;
        DWORD       cbEmfData = 0;
    };

    BOOL ParseEmfSpoolPages(const BYTE* pData, DWORD cbData,
                             std::vector<PageData>& pages);

    BOOL RenderPages(HDC hDC, const std::vector<PageData>& pages,
                     const wchar_t* pDocName);

    DEVMODEW* BuildMergedDevMode(HANDLE hPrinter, const CPrinterSettings& settings,
                                  std::vector<BYTE>& dmBuf);
};
