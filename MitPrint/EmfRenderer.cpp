#include <afxwin.h>
#include "EmfRenderer.h"
#include "MitLog.h"

BOOL CEmfRenderer::RenderToPdf(const wchar_t* pDocName,
                                const BYTE*    pSpoolData,
                                DWORD          cbSpoolData,
                                const wchar_t* pOutputPath)
{
    if (!pOutputPath || !pOutputPath[0]) { MitLog("RenderToPdf: empty output path"); return FALSE; }
    if (!pSpoolData || cbSpoolData == 0)  { MitLog("RenderToPdf: no spool data"); return FALSE; }

    MitLog("RenderToPdf: outputPath=%S cbSpoolData=%u", pOutputPath, cbSpoolData);
    HDC hDC = CreateDCW(L"WINSPOOL", L"Microsoft Print To PDF", nullptr, nullptr);
    if (!hDC) { MitLog("RenderToPdf: CreateDC failed err=%u", GetLastError()); return FALSE; }

    std::vector<PageData> pages;
    BOOL ok = ParseEmfSpoolPages(pSpoolData, cbSpoolData, pages);
    MitLog("RenderToPdf: ParseEmfSpoolPages ok=%d pages=%u", ok, (DWORD)pages.size());
    if (ok && !pages.empty())
    {
        DOCINFOW di = {};
        di.cbSize      = sizeof(di);
        di.lpszDocName = pDocName ? pDocName : L"MitPrint Job";
        di.lpszOutput  = pOutputPath;

        int docRet = StartDocW(hDC, &di);
        MitLog("RenderToPdf: StartDocW returned %d err=%u", docRet, GetLastError());
        if (docRet > 0)
        {
            int pageIdx = 0;
            for (const PageData& pg : pages)
            {
                if (StartPage(hDC) <= 0) { MitLog("RenderToPdf: StartPage failed"); break; }

                // Dump EMF header fields for diagnosis
                DWORD first4 = pg.cbEmfData >= 4 ? *(const DWORD*)pg.pEmfData : 0;
                MitLog("RenderToPdf: page[%d] cbEmfData=%u first4=0x%08X", pageIdx, pg.cbEmfData, first4);
                if (pg.cbEmfData >= 52)
                {
                    DWORD nSize     = *(const DWORD*)(pg.pEmfData + 4);
                    DWORD sig       = *(const DWORD*)(pg.pEmfData + 40);
                    DWORD nBytes    = *(const DWORD*)(pg.pEmfData + 48);
                    DWORD nRecords  = *(const DWORD*)(pg.pEmfData + 52);
                    MitLog("  EMF hdr: nSize=%u sig=0x%08X nBytes=%u nRecords=%u cbEmfData=%u",
                           nSize, sig, nBytes, nRecords, pg.cbEmfData);
                }

                // nBytes in EMR_HEADER is set by the spooler to cjSize (includes the 8-byte
                // spool record header). We only have the data portion, so patch nBytes to match.
                std::vector<BYTE> emfBuf(pg.pEmfData, pg.pEmfData + pg.cbEmfData);
                if (emfBuf.size() >= 52)
                {
                    DWORD& nBytes = *(DWORD*)(emfBuf.data() + 48);
                    if (nBytes != (DWORD)emfBuf.size())
                    {
                        MitLog("RenderToPdf: patch nBytes %u->%u", nBytes, (DWORD)emfBuf.size());
                        nBytes = (DWORD)emfBuf.size();
                    }
                }

                HENHMETAFILE hEmf = SetEnhMetaFileBits((DWORD)emfBuf.size(), emfBuf.data());
                MitLog("RenderToPdf: SetEnhMetaFileBits hEmf=%p err=%u", hEmf, hEmf ? 0u : GetLastError());
                if (hEmf)
                {
                    RECT bounds = {};
                    GetClipBox(hDC, &bounds);
                    if (bounds.right == 0)
                    {
                        bounds.right  = GetDeviceCaps(hDC, HORZRES);
                        bounds.bottom = GetDeviceCaps(hDC, VERTRES);
                    }
                    MitLog("RenderToPdf: bounds=%d,%d,%d,%d", bounds.left, bounds.top, bounds.right, bounds.bottom);
                    BOOL playOk = PlayEnhMetaFile(hDC, hEmf, &bounds);
                    MitLog("RenderToPdf: PlayEnhMetaFile ok=%d err=%u", playOk, playOk ? 0u : GetLastError());
                    DeleteEnhMetaFile(hEmf);
                }
                EndPage(hDC);
                ++pageIdx;
            }
            EndDoc(hDC);
        }
        else { MitLog("RenderToPdf: StartDocW failed"); ok = FALSE; }
    }

    DeleteDC(hDC);
    return ok;
}

BOOL CEmfRenderer::RenderToRealPrinter(const wchar_t* pTargetPrinter,
                                        const wchar_t* pDocName,
                                        const BYTE*    pSpoolData,
                                        DWORD          cbSpoolData,
                                        const CPrinterSettings& settings)
{
    if (!pTargetPrinter || !pTargetPrinter[0]) { MitLog("RenderToRealPrinter: empty printer name"); return FALSE; }
    if (!pSpoolData || cbSpoolData == 0)       { MitLog("RenderToRealPrinter: no spool data"); return FALSE; }

    MitLog("RenderToRealPrinter: printer=%S cbSpoolData=%u", pTargetPrinter, cbSpoolData);

    // Open the real target printer
    HANDLE hPrinter = nullptr;
    if (!OpenPrinterW((LPWSTR)pTargetPrinter, &hPrinter, nullptr))
    {
        MitLog("RenderToRealPrinter: OpenPrinter failed err=%u", GetLastError());
        return FALSE;
    }

    // Build merged DEVMODE (target printer defaults + user settings)
    std::vector<BYTE> dmBuf;
    DEVMODEW* pDm = BuildMergedDevMode(hPrinter, settings, dmBuf);

    // Create DC for the real printer
    HDC hDC = CreateDCW(L"WINSPOOL", pTargetPrinter, nullptr, pDm);
    if (!hDC)
    {
        MitLog("RenderToRealPrinter: CreateDC failed err=%u", GetLastError());
        ClosePrinter(hPrinter);
        return FALSE;
    }

    // Parse the EMF spool stream to extract page EMFs
    std::vector<PageData> pages;
    BOOL ok = ParseEmfSpoolPages(pSpoolData, cbSpoolData, pages);
    MitLog("RenderToRealPrinter: ParseEmfSpoolPages ok=%d pages=%u", ok, (DWORD)pages.size());

    if (ok && !pages.empty())
    {
        ok = RenderPages(hDC, pages, pDocName);
        MitLog("RenderToRealPrinter: RenderPages ok=%d", ok);
    }

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
    if (cbData < sizeof(EmfSpoolFileHeader))
    {
        MitLog("ParseEmf: too small cbData=%u", cbData);
        return FALSE;
    }

    const EmfSpoolFileHeader* pFH = (const EmfSpoolFileHeader*)pData;
    MitLog("ParseEmf: hdr dwVersion=0x%08X cjSize=%u", pFH->dwVersion, pFH->cjSize);

    // Dump first 32 bytes as hex for diagnosis
    {
        char hex[128] = {};
        DWORD n = cbData < 32u ? cbData : 32u;
        for (DWORD i = 0; i < n; i++)
            _snprintf_s(hex + i * 3, sizeof(hex) - i * 3, _TRUNCATE, "%02X ", pData[i]);
        MitLog("ParseEmf: bytes[0..31]: %s", hex);
    }

    DWORD offset = pFH->cjSize;
    if (offset >= cbData)
    {
        MitLog("ParseEmf: cjSize=%u >= cbData=%u, resetting to 0", offset, cbData);
        offset = 0;
    }

    int nRec = 0;
    while (offset + sizeof(EmfSpoolRecord) <= cbData)
    {
        const EmfSpoolRecord* rec = (const EmfSpoolRecord*)(pData + offset);
        MitLog("ParseEmf: rec[%d] offset=%u ulID=%u cjSize=%u",
               nRec++, offset, rec->ulID, rec->cjSize);

        if (rec->cjSize < sizeof(EmfSpoolRecord)) { MitLog("ParseEmf: corrupt"); break; }
        if (offset + rec->cjSize > cbData)         { MitLog("ParseEmf: truncated"); break; }
        if (rec->ulID == EMRI_EOF)                  { MitLog("ParseEmf: EOF"); break; }

        if (rec->ulID == EMRI_METAFILE      ||
            rec->ulID == EMRI_METAFILE_DATA ||
            rec->ulID == EMRI_METAFILE_EXT)
        {
            PageData pg;
            pg.pEmfData  = pData + offset + sizeof(EmfSpoolRecord);
            pg.cbEmfData = rec->cjSize - sizeof(EmfSpoolRecord);
            MitLog("ParseEmf: PAGE record type=%u dataSize=%u", rec->ulID, pg.cbEmfData);
            if (pg.cbEmfData > 0)
                pages.push_back(pg);
        }

        offset += rec->cjSize;
        offset = (offset + 3) & ~3u;

        if (nRec > 200) { MitLog("ParseEmf: too many records"); break; }
    }

    MitLog("ParseEmf: done, pages=%u", (DWORD)pages.size());

    // Fallback: treat entire buffer as a single raw EMF (plain EMF file)
    if (pages.empty() && cbData >= 4)
    {
        // EMF magic: first DWORD of an EMR_HEADER record is type=1
        if (*(const DWORD*)pData == 1 /*EMR_HEADER*/)
        {
            MitLog("ParseEmf: fallback raw EMF cbData=%u", cbData);
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

        std::vector<BYTE> emfBuf(pg.pEmfData, pg.pEmfData + pg.cbEmfData);
        if (emfBuf.size() >= 52)
        {
            DWORD& nBytes = *(DWORD*)(emfBuf.data() + 48);
            if (nBytes != (DWORD)emfBuf.size()) nBytes = (DWORD)emfBuf.size();
        }
        HENHMETAFILE hEmf = SetEnhMetaFileBits((DWORD)emfBuf.size(), emfBuf.data());
        if (hEmf)
        {
            RECT bounds = {};
            GetClipBox(hDC, &bounds);

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
