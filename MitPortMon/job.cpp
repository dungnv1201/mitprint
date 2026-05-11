#include "job.h"
#include "port.h"
#include "pipe_client.h"
#include "protocol.h"

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

        LeaveCriticalSection(&ctx->cs);
        return ok;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return FALSE; }
}

// ---------------------------------------------------------------
// WritePort — called repeatedly with EMF spool data chunks
// ---------------------------------------------------------------
BOOL WINAPI MitWritePort(HANDLE hPort, LPBYTE pBuffer,
                          DWORD cbBuf, LPDWORD pcbWritten)
{
    __try
    {
        if (pcbWritten) *pcbWritten = 0;
        if (!pBuffer || cbBuf == 0) return TRUE;

        MitPortContext* ctx = (MitPortContext*)hPort;
        if (!ctx || ctx->signature != 'MITP') return FALSE;
        if (ctx->hPipe == INVALID_HANDLE_VALUE) return FALSE;

        EnterCriticalSection(&ctx->cs);

        // Chunk large buffers into pipe-sized pieces
        DWORD offset    = 0;
        DWORD remaining = cbBuf;
        BOOL  ok        = TRUE;

        while (remaining > 0 && ok)
        {
            DWORD maxChunk  = MIT_PIPE_BUF_SIZE - (DWORD)(sizeof(MitMsgHeader) + sizeof(MitDataChunkPayload));
            DWORD chunkSize = remaining < maxChunk ? remaining : maxChunk;

            MitMsgHeader hdr = {};
            hdr.magic       = MIT_MSG_MAGIC;
            hdr.version     = MIT_PROTOCOL_VER;
            hdr.type        = MitMsgType::DataChunk;
            hdr.jobId       = ctx->currentJobId;
            hdr.sequence    = ctx->sequenceNum++;
            hdr.payloadSize = sizeof(MitDataChunkPayload) + chunkSize;

            MitDataChunkPayload cp = {};
            cp.offset    = offset;
            cp.chunkSize = chunkSize;

            ok = MitPipeSendChunk(ctx->hPipe, &hdr, &cp, pBuffer + offset, chunkSize);
            if (ok)
            {
                offset    += chunkSize;
                remaining -= chunkSize;
            }
        }

        if (ok && pcbWritten)
            *pcbWritten = cbBuf;

        LeaveCriticalSection(&ctx->cs);
        return ok;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return FALSE; }
}

// ---------------------------------------------------------------
// EndDocPort — signals job data complete
// ---------------------------------------------------------------
BOOL WINAPI MitEndDocPort(HANDLE hPort)
{
    __try
    {
        MitPortContext* ctx = (MitPortContext*)hPort;
        if (!ctx || ctx->signature != 'MITP') return FALSE;
        if (ctx->hPipe == INVALID_HANDLE_VALUE) return FALSE;

        EnterCriticalSection(&ctx->cs);

        MitMsgHeader hdr = {};
        hdr.magic       = MIT_MSG_MAGIC;
        hdr.version     = MIT_PROTOCOL_VER;
        hdr.type        = MitMsgType::EndJob;
        hdr.jobId       = ctx->currentJobId;
        hdr.sequence    = ctx->sequenceNum++;
        hdr.payloadSize = 0;

        MitPipeSend(ctx->hPipe, &hdr, NULL, 0);

        ctx->jobActive = FALSE;
        LeaveCriticalSection(&ctx->cs);
        return TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return FALSE; }
}
