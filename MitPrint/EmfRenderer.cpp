#include <afxwin.h>
#include "EmfRenderer.h"

BOOL CEmfRenderer::RenderToRealPrinter(const wchar_t* pTargetPrinter,
                                        const wchar_t* pDocName,
                                        const BYTE*    pSpoolData,
                                        DWORD          cbSpoolData,
                                        const CPrinterSettings& settings)
{
    if (!pTargetPrinter || !pTargetPrinter[0]) return FALSE;
    if (!pSpoolData || cbSpoolData == 0)        return FALSE;

    // Open the real target printer
    HANDLE hPrinter = nullptr;
    if (!OpenPrinterW((LPWSTR)pTargetPrinter, &hPrinter, nullptr))
        return FALSE;

    // Build merged DEVMODE (target printer defaults + user settings)
    std::vector<BYTE> dmBuf;
    DEVMODEW* pDm = BuildMergedDevMode(hPrinter, settings, dmBuf);

    // Create DC for the real printer
    HDC hDC = CreateDCW(L"WINSPOOL", pTargetPrinter, nullptr, pDm);
    if (!hDC)
    {
        ClosePrinter(hPrinter);
        return FALSE;
    }

    // Parse the EMF spool stream to extract page EMFs
    std::vector<PageData> pages;
    BOOL ok = ParseEmfSpoolPages(pSpoolData, cbSpoolData, pages);

    if (ok && !pages.empty())
        ok = RenderPages(hDC, pages, pDocName);

    DeleteDC(hDC);
    ClosePrinter(hPrinter);
    return ok;
}

// ---------------------------------------------------------------
// Parse [MS-EMFSPOOL] stream and extract per-page EMF blobs
// ---------------------------------------------------------------
BOOL CEmfRenderer::ParseEmfSpoolPages(const BYTE* pData, DWORD cbData,
                                       std::vector<PageData>& pages)
{
    if (cbData < sizeof(EmfSpoolFileHeader)) return FALSE;

    const EmfSpoolFileHeader* pFH = (const EmfSpoolFileHeader*)pData;

    // Walk records after the header
    DWORD offset = pFH->cjSize;
    if (offset >= cbData) offset = 0; // malformed or raw EMF, try from start

    while (offset + sizeof(EmfSpoolRecord) <= cbData)
    {
        const EmfSpoolRecord* rec = (const EmfSpoolRecord*)(pData + offset);

        if (rec->cjSize < sizeof(EmfSpoolRecord)) break; // corrupt
        if (offset + rec->cjSize > cbData) break;       // truncated

        if (rec->ulID == EMRI_EOF) break;

        if (rec->ulID == EMRI_METAFILE_DATA || rec->ulID == EMRI_METAFILE_EXT)
        {
            PageData pg;
            pg.pEmfData  = pData + offset + sizeof(EmfSpoolRecord);
            pg.cbEmfData = rec->cjSize - sizeof(EmfSpoolRecord);
            if (pg.cbEmfData > 0)
                pages.push_back(pg);
        }

        offset += rec->cjSize;
        // Records are DWORD-aligned
        offset = (offset + 3) & ~3u;
    }

    // Fallback: treat entire buffer as a single EMF (plain file from Notepad etc.)
    if (pages.empty() && cbData >= 4)
    {
        // Check for EMF magic: first record is EMR_HEADER (type 1)
        if (*(DWORD*)pData == 1 /*EMR_HEADER*/)
        {
            PageData pg;
            pg.pEmfData  = pData;
            pg.cbEmfData = cbData;
            pages.push_back(pg);
        }
    }

    return !pages.empty();
}

// ---------------------------------------------------------------
// Render all pages to the printer DC
// ---------------------------------------------------------------
BOOL CEmfRenderer::RenderPages(HDC hDC, const std::vector<PageData>& pages,
                                const wchar_t* pDocName)
{
    DOCINFOW di = {};
    di.cbSize      = sizeof(di);
    di.lpszDocName = pDocName ? pDocName : L"MitPrint Job";

    if (StartDocW(hDC, &di) <= 0) return FALSE;

    for (const PageData& pg : pages)
    {
        if (StartPage(hDC) <= 0) break;

        HENHMETAFILE hEmf = SetEnhMetaFileBits(pg.cbEmfData, pg.pEmfData);
        if (hEmf)
        {
            RECT bounds = {};
            GetClipBox(hDC, &bounds);

            // Use physical page dimensions if clip box is empty
            if (bounds.right == 0)
            {
                bounds.right  = GetDeviceCaps(hDC, HORZRES);
                bounds.bottom = GetDeviceCaps(hDC, VERTRES);
            }

            PlayEnhMetaFile(hDC, hEmf, &bounds);
            DeleteEnhMetaFile(hEmf);
        }

        EndPage(hDC);
    }

    EndDoc(hDC);
    return TRUE;
}

// ---------------------------------------------------------------
// Build a DEVMODE that merges printer defaults with user settings
// ---------------------------------------------------------------
DEVMODEW* CEmfRenderer::BuildMergedDevMode(HANDLE hPrinter,
                                            const CPrinterSettings& settings,
                                            std::vector<BYTE>& dmBuf)
{
    // Get required buffer size
    DWORD needed = DocumentPropertiesW(nullptr, hPrinter, nullptr, nullptr, nullptr, 0);
    if (needed <= 0) return nullptr;

    dmBuf.resize(needed, 0);
    DEVMODEW* pDm = (DEVMODEW*)dmBuf.data();

    // Get printer defaults
    if (DocumentPropertiesW(nullptr, hPrinter, nullptr, pDm, nullptr, DM_OUT_BUFFER) != IDOK)
        return nullptr;

    // Apply user settings
    pDm->dmFields |= DM_PAPERSIZE | DM_COPIES | DM_DUPLEX | DM_ORIENTATION;
    pDm->dmPaperSize   = (short)settings.paperSize;
    pDm->dmCopies      = settings.copies > 0 ? settings.copies : 1;
    pDm->dmDuplex      = settings.duplex;
    pDm->dmOrientation = settings.orientation;

    // Merge back into printer
    std::vector<BYTE> outBuf(needed, 0);
    DEVMODEW* pOut = (DEVMODEW*)outBuf.data();
    if (DocumentPropertiesW(nullptr, hPrinter, nullptr, pOut, pDm,
                            DM_IN_BUFFER | DM_OUT_BUFFER) == IDOK)
    {
        dmBuf = std::move(outBuf);
        return (DEVMODEW*)dmBuf.data();
    }

    return pDm;
}
