#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winspool.h>
#include <vector>
#include "PrinterSettingsStore.h"

// EMF spool record types (from [MS-EMFSPOOL] specification)
enum EmfSpoolRecordType : DWORD
{
    EMRI_METAFILE_DATA      = 1,
    EMRI_FORM_METAFILE      = 2,
    EMRI_BW_METAFILE        = 3,
    EMRI_METAFILE_EXT       = 4,
    EMRI_BW_METAFILE_EXT    = 5,
    EMRI_DESIGNVECTOR       = 6,
    EMRI_SUBSET_FONT        = 7,
    EMRI_DELTA_FONT         = 8,
    EMRI_FORM_METAFILE_EXT  = 9,
    EMRI_PRESTARTPAGE       = 10,
    EMRI_CLIP_METAFILE_EXT  = 11,
    EMRI_POSTSCRIPT         = 14,
    EMRI_EOF                = 0,
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
