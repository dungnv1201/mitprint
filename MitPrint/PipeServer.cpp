#include <afxwin.h>
#include "PipeServer.h"
#include "protocol.h"
#include <sddl.h>

CPipeServer::CPipeServer()
{
}

CPipeServer::~CPipeServer()
{
    Stop();
}

BOOL CPipeServer::Start(HWND hNotifyWnd, CJobProcessor* pJobProc)
{
    m_hNotifyWnd = hNotifyWnd;
    m_pJobProc   = pJobProc;
    m_bRunning   = TRUE;

    m_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_hThread    = CreateThread(nullptr, 0, IoThreadProc, this, 0, nullptr);
    return m_hThread != nullptr;
}

void CPipeServer::Stop()
{
    m_bRunning = FALSE;
    if (m_hStopEvent) SetEvent(m_hStopEvent);
    if (m_hThread)
    {
        WaitForSingleObject(m_hThread, 3000);
        CloseHandle(m_hThread);
        m_hThread = nullptr;
    }
    if (m_hStopEvent)
    {
        CloseHandle(m_hStopEvent);
        m_hStopEvent = nullptr;
    }
}

DWORD WINAPI CPipeServer::IoThreadProc(LPVOID pThis)
{
    ((CPipeServer*)pThis)->IoThread();
    return 0;
}

void CPipeServer::IoThread()
{
    // Receive messages from one pipe connection at a time (simple sequential model)
    // For high throughput, upgrade to IOCP; this handles typical print workloads.
    while (m_bRunning)
    {
        SECURITY_ATTRIBUTES sa = {};
        PSECURITY_DESCRIPTOR pSd = nullptr;
        BuildPipeSecurityAttr(&sa, &pSd);

        HANDLE hPipe = CreateNamedPipeW(
            MIT_PIPE_NAME,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            MIT_PIPE_BUF_SIZE, MIT_PIPE_BUF_SIZE,
            0,
            pSd ? &sa : nullptr);

        if (pSd) LocalFree(pSd);

        if (hPipe == INVALID_HANDLE_VALUE) break;

        // Overlapped connect
        OVERLAPPED ov = {};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        BOOL bConnected = ConnectNamedPipe(hPipe, &ov);
        if (!bConnected && GetLastError() == ERROR_IO_PENDING)
        {
            HANDLE waitHandles[2] = { ov.hEvent, m_hStopEvent };
            DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
            if (waitRes != WAIT_OBJECT_0)
            {
                CloseHandle(ov.hEvent);
                CloseHandle(hPipe);
                break;
            }
            DWORD transferred = 0;
            GetOverlappedResult(hPipe, &ov, &transferred, FALSE);
            bConnected = TRUE;
        }
        else if (GetLastError() == ERROR_PIPE_CONNECTED)
        {
            bConnected = TRUE;
        }

        CloseHandle(ov.hEvent);

        if (!bConnected)
        {
            CloseHandle(hPipe);
            continue;
        }

        // Read messages until client disconnects
        std::vector<BYTE> buf(MIT_PIPE_BUF_SIZE * 4);
        while (m_bRunning)
        {
            DWORD bytesRead = 0;
            BOOL ok = ReadFile(hPipe, buf.data(), (DWORD)buf.size(), &bytesRead, nullptr);
            if (!ok || bytesRead == 0) break;

            DispatchMessage(buf.data(), bytesRead);
        }

        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
}

void CPipeServer::DispatchMessage(const BYTE* pBuf, DWORD cbBuf)
{
    if (cbBuf < sizeof(MitMsgHeader)) return;

    const MitMsgHeader* hdr = (const MitMsgHeader*)pBuf;
    if (hdr->magic != MIT_MSG_MAGIC || hdr->version != MIT_PROTOCOL_VER) return;

    const BYTE* payload = pBuf + sizeof(MitMsgHeader);
    DWORD payloadSize   = cbBuf - sizeof(MitMsgHeader);

    switch (hdr->type)
    {
    case MitMsgType::StartJob:
    {
        if (payloadSize < sizeof(MitStartJobPayload)) break;
        const MitStartJobPayload* p = (const MitStartJobPayload*)payload;
        m_pJobProc->OnStartJob(hdr->jobId, p->documentName, p->userName);
        break;
    }
    case MitMsgType::DataChunk:
    {
        if (payloadSize < sizeof(MitDataChunkPayload)) break;
        const MitDataChunkPayload* p = (const MitDataChunkPayload*)payload;
        const BYTE* data = payload + sizeof(MitDataChunkPayload);
        DWORD dataSize   = payloadSize - sizeof(MitDataChunkPayload);
        if (dataSize == p->chunkSize)
            m_pJobProc->OnDataChunk(hdr->jobId, p->offset, data, dataSize);
        break;
    }
    case MitMsgType::EndJob:
        m_pJobProc->OnEndJob(hdr->jobId);
        break;
    default:
        break;
    }
}

BOOL CPipeServer::BuildPipeSecurityAttr(SECURITY_ATTRIBUTES* pSa, PSECURITY_DESCRIPTOR* ppSd)
{
    // Allow SYSTEM (spoolsv.exe) and Authenticated Users to read/write
    BOOL ok = ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:(A;;GRGWGX;;;SY)(A;;GRGWGX;;;AU)",
        SDDL_REVISION_1,
        ppSd, nullptr);

    if (ok && ppSd && *ppSd)
    {
        pSa->nLength              = sizeof(SECURITY_ATTRIBUTES);
        pSa->lpSecurityDescriptor = *ppSd;
        pSa->bInheritHandle       = FALSE;
    }
    return ok;
}
