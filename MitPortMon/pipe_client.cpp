#include "pipe_client.h"

static const int kMaxRetries = 3;

HANDLE MitPipeConnect()
{
    for (int attempt = 0; attempt < kMaxRetries; ++attempt)
    {
        HANDLE hPipe = CreateFileW(
            MIT_PIPE_NAME,
            GENERIC_READ | GENERIC_WRITE,
            0, NULL,
            OPEN_EXISTING,
            0, NULL);

        if (hPipe != INVALID_HANDLE_VALUE)
        {
            DWORD mode = PIPE_READMODE_MESSAGE;
            SetNamedPipeHandleState(hPipe, &mode, NULL, NULL);
            return hPipe;
        }

        if (GetLastError() != ERROR_PIPE_BUSY)
            break;

        WaitNamedPipeW(MIT_PIPE_NAME, MIT_PIPE_TIMEOUT);
    }
    return INVALID_HANDLE_VALUE;
}

BOOL MitPipeSend(HANDLE hPipe, const MitMsgHeader* pHdr,
                 const void* pPayload, DWORD cbPayload)
{
    if (hPipe == INVALID_HANDLE_VALUE || !pHdr) return FALSE;

    // Compose into a single buffer to avoid partial writes
    DWORD totalSize = sizeof(MitMsgHeader) + cbPayload;
    BYTE* buf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, totalSize);
    if (!buf) return FALSE;

    CopyMemory(buf, pHdr, sizeof(MitMsgHeader));
    if (pPayload && cbPayload)
        CopyMemory(buf + sizeof(MitMsgHeader), pPayload, cbPayload);

    DWORD written = 0;
    BOOL ok = WriteFile(hPipe, buf, totalSize, &written, NULL);
    HeapFree(GetProcessHeap(), 0, buf);
    return ok && (written == totalSize);
}

BOOL MitPipeSendChunk(HANDLE hPipe, const MitMsgHeader* pHdr,
                      const MitDataChunkPayload* pChunk,
                      const BYTE* pData, DWORD cbData)
{
    if (hPipe == INVALID_HANDLE_VALUE || !pHdr || !pChunk) return FALSE;

    DWORD totalSize = sizeof(MitMsgHeader) + sizeof(MitDataChunkPayload) + cbData;
    BYTE* buf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, totalSize);
    if (!buf) return FALSE;

    BYTE* p = buf;
    CopyMemory(p, pHdr,   sizeof(MitMsgHeader));        p += sizeof(MitMsgHeader);
    CopyMemory(p, pChunk, sizeof(MitDataChunkPayload)); p += sizeof(MitDataChunkPayload);
    if (pData && cbData)
        CopyMemory(p, pData, cbData);

    DWORD written = 0;
    BOOL ok = WriteFile(hPipe, buf, totalSize, &written, NULL);
    HeapFree(GetProcessHeap(), 0, buf);
    return ok && (written == totalSize);
}
