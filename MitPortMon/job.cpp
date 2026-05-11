#include "job.h"
#include "port.h"
#include "pipe_client.h"
#include "protocol.h"
#include <strsafe.h>
#include <stdio.h>

// Simple log function — runs as SYSTEM, writes to C:\Windows\Temp\MitMon.log
static void MonLog(const char* fmt, ...)
{
    char msg[512];
    va_list args;
    va_start(args, fmt);
    _vsnprintf_s(msg, sizeof(msg), _TRUNCATE, fmt, args);
    va_end(args);

    OutputDebugStringA("[MitMon] ");
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");

    HANDLE hFile = CreateFileW(L"C:\\Windows\\Temp\\MitMon.log",
                               FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    char line[600];
    _snprintf_s(line, sizeof(line), _TRUNCATE,
                "[%02d:%02d:%02d.%03d] %s\r\n",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
    DWORD written = 0;
    WriteFile(hFile, line, (DWORD)strlen(line), &written, nullptr);
    CloseHandle(hFile);
}

// ---------------------------------------------------------------
// StartDocPort — called by spooler once per print job
// ---------------------------------------------------------------
BOOL WINAPI MitStartDocPort(HANDLE hPort, LPWSTR pPrinterName,
                             DWORD JobId, DWORD Level, LPBYTE pDocInfo)
{
    __try
    {
        MitPortContext* ctx = (MitPortContext*)hPort;
        if (!ctx || ctx->signature != 'MITP') return FALSE;

        EnterCriticalSection(&ctx->cs);

        // Reconnect pipe if needed
        if (ctx->hPipe == INVALID_HANDLE_VALUE)
            ctx->hPipe = MitPipeConnect();

        if (ctx->hPipe == INVALID_HANDLE_VALUE)
        {
            LeaveCriticalSection(&ctx->cs);
            return FALSE;
        }

        ctx->currentJobId = JobId;
        ctx->sequenceNum  = 0;
        ctx->jobActive    = TRUE;
        MonLog("StartDocPort: jobId=%u printer=%S", JobId, pPrinterName ? pPrinterName : L"<null>");

        // Build StartJob message
        MitMsgHeader hdr = {};
        hdr.magic       = MIT_MSG_MAGIC;
        hdr.version     = MIT_PROTOCOL_VER;
        hdr.type        = MitMsgType::StartJob;
        hdr.jobId       = JobId;
        hdr.sequence    = ctx->sequenceNum++;

        MitStartJobPayload payload = {};
        payload.totalPages = 0;

        // Extract document name from DOC_INFO_1 if available
        if (Level == 1 && pDocInfo)
        {
            DOC_INFO_1W* di = (DOC_INFO_1W*)pDocInfo;
            if (di->pDocName)
                lstrcpynW(payload.documentName, di->pDocName,
                          sizeof(payload.documentName) / sizeof(WCHAR) - 1);
        }

        // Try to get current user
        DWORD userLen = sizeof(payload.userName) / sizeof(WCHAR) - 1;
        GetUserNameW(payload.userName, &userLen);

        hdr.payloadSize = sizeof(payload);

        BOOL ok = MitPipeSend(ctx->hPipe, &hdr, &payload, sizeof(payload));
        if (!ok)
        {
            // Attempt reconnect once
            CloseHandle(ctx->hPipe);
            ctx->hPipe = MitPipeConnect();
            if (ctx->hPipe != INVALID_HANDLE_VALUE)
                ok = MitPipeSend(ctx->hPipe, &hdr, &payload, sizeof(payload));
        }

        // -------------------------------------------------------
        // Read the raw .SPL spool file NOW, while WinPrint is still
        // inside PrintDocumentOnPrintProcessor and the file exists.
        // By EndDocPort time WinPrint has already deleted the file.
        // -------------------------------------------------------
        if (ok)
        {
            // Drop any thread impersonation so we can access the spool dir as SYSTEM
            HANDLE hImpToken = nullptr;
            OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE | TOKEN_DUPLICATE,
                            FALSE, &hImpToken);
            if (hImpToken) RevertToSelf();

            wchar_t sysDir[MAX_PATH] = {};
            GetSystemDirectoryW(sysDir, MAX_PATH);

            // Determine spool directory from registry (fallback to default)
            wchar_t baseDir[MAX_PATH] = {};
            {
                DWORD cbReg = sizeof(baseDir);
                if (RegGetValueW(HKEY_LOCAL_MACHINE,
                        L"SYSTEM\\CurrentControlSet\\Control\\Print\\Printers",
                        L"DefaultSpoolDirectory", RRF_RT_REG_SZ,
                        nullptr, baseDir, &cbReg) != ERROR_SUCCESS || !baseDir[0])
                    StringCchPrintfW(baseDir, MAX_PATH, L"%s\\spool\\PRINTERS", sysDir);
                MonLog("StartDocPort: spoolDir=%S", baseDir);
            }

            // Find the newest .SPL file — the spool file number does NOT equal JobId on Win11
            wchar_t spoolFile[MAX_PATH] = {};
            {
                wchar_t searchPath[MAX_PATH] = {};
                StringCchPrintfW(searchPath, MAX_PATH, L"%s\\*.SPL", baseDir);
                WIN32_FIND_DATAW fd = {};
                FILETIME ftNewest = {};
                HANDLE hFind = FindFirstFileW(searchPath, &fd);
                if (hFind != INVALID_HANDLE_VALUE) {
                    do {
                        MonLog("StartDocPort: found SPL: %S size=%u", fd.cFileName, fd.nFileSizeLow);
                        if (CompareFileTime(&fd.ftLastWriteTime, &ftNewest) > 0)
                        {
                            ftNewest = fd.ftLastWriteTime;
                            StringCchPrintfW(spoolFile, MAX_PATH, L"%s\\%s", baseDir, fd.cFileName);
                        }
                    } while (FindNextFileW(hFind, &fd));
                    FindClose(hFind);
                    MonLog("StartDocPort: using spoolFile=%S", spoolFile);
                } else {
                    MonLog("StartDocPort: FindFirstFile failed err=%u", GetLastError());
                }
            }

            MonLog("StartDocPort: reading spoolFile=%S", spoolFile);

            HANDLE hFile = CreateFileW(spoolFile, GENERIC_READ,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       NULL, OPEN_EXISTING, 0, NULL);

            if (hFile != INVALID_HANDLE_VALUE)
            {
                DWORD fileSize = GetFileSize(hFile, nullptr);
                MonLog("StartDocPort: spool file size=%u", fileSize);

                const DWORD maxChunk = MIT_PIPE_BUF_SIZE
                                       - (DWORD)(sizeof(MitMsgHeader) + sizeof(MitDataChunkPayload));
                BYTE* dataBuf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, maxChunk);

                if (dataBuf)
                {
                    DWORD sendOffset = 0;
                    DWORD cbRead = 0;

                    while (ReadFile(hFile, dataBuf, maxChunk, &cbRead, NULL) && cbRead > 0)
                    {
                        MitMsgHeader chunkHdr = {};
                        chunkHdr.magic       = MIT_MSG_MAGIC;
                        chunkHdr.version     = MIT_PROTOCOL_VER;
                        chunkHdr.type        = MitMsgType::DataChunk;
                        chunkHdr.jobId       = JobId;
                        chunkHdr.sequence    = ctx->sequenceNum++;
                        chunkHdr.payloadSize = sizeof(MitDataChunkPayload) + cbRead;

                        MitDataChunkPayload cp = {};
                        cp.offset    = sendOffset;
                        cp.chunkSize = cbRead;

                        if (!MitPipeSendChunk(ctx->hPipe, &chunkHdr, &cp, dataBuf, cbRead)) break;
                        sendOffset += cbRead;
                    }

                    MonLog("StartDocPort: sent %u bytes from spool file", sendOffset);
                    HeapFree(GetProcessHeap(), 0, dataBuf);
                }
                CloseHandle(hFile);
            }
            else
            {
                MonLog("StartDocPort: cannot open spool file err=%u", GetLastError());
            }

            // Restore impersonation if we reverted it
            if (hImpToken)
            {
                SetThreadToken(nullptr, hImpToken);
                CloseHandle(hImpToken);
            }
        }

        LeaveCriticalSection(&ctx->cs);
        return ok;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return FALSE; }
}

// ---------------------------------------------------------------
// WritePort — called by WinPrint with UNIDRV-rendered output.
// We ignore this data because it comes from UNIDRV's printer
// language rendering (which generates almost nothing without GPD
// output commands). The real EMF spool data is read from the .SPL
// file in EndDocPort instead.
// ---------------------------------------------------------------
BOOL WINAPI MitWritePort(HANDLE hPort, LPBYTE pBuffer,
                          DWORD cbBuf, LPDWORD pcbWritten)
{
    __try
    {
        if (pcbWritten) *pcbWritten = cbBuf; // acknowledge all bytes
        return TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return FALSE; }
}

// ---------------------------------------------------------------
// EndDocPort — reads the raw .SPL spool file and sends its content
// as DataChunk messages, then sends EndJob.
//
// Why here: the port monitor runs as SYSTEM inside spoolsv.exe, so
// it can read any spool file. The .SPL file is the EMF spool (MS-
// EMFSPOOL format) — exactly what EmfRenderer expects. UNIDRV's
// WritePort output is discarded because the GPD has no printer
// language commands, so WritePort only produces garbage/nothing.
// ---------------------------------------------------------------
BOOL WINAPI MitEndDocPort(HANDLE hPort)
{
    __try
    {
        MitPortContext* ctx = (MitPortContext*)hPort;
        if (!ctx || ctx->signature != 'MITP') return FALSE;
        if (ctx->hPipe == INVALID_HANDLE_VALUE) return FALSE;

        EnterCriticalSection(&ctx->cs);

        MonLog("EndDocPort: jobId=%u sending EndJob", ctx->currentJobId);

        // Send EndJob
        MitMsgHeader endHdr = {};
        endHdr.magic       = MIT_MSG_MAGIC;
        endHdr.version     = MIT_PROTOCOL_VER;
        endHdr.type        = MitMsgType::EndJob;
        endHdr.jobId       = ctx->currentJobId;
        endHdr.sequence    = ctx->sequenceNum++;
        endHdr.payloadSize = 0;

        MitPipeSend(ctx->hPipe, &endHdr, NULL, 0);

        ctx->jobActive = FALSE;
        LeaveCriticalSection(&ctx->cs);
        return TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return FALSE; }
}
