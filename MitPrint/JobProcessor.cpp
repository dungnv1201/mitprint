#include <afxwin.h>
#include "JobProcessor.h"
#include "EmfRenderer.h"
#include "PrinterSettingsStore.h"
#include "mit_guids.h"

CJobProcessor::CJobProcessor()
{
    InitializeCriticalSection(&m_cs);
}

CJobProcessor::~CJobProcessor()
{
    m_bStop = TRUE;
    if (m_hWorkEvent)  SetEvent(m_hWorkEvent);
    if (m_hWorkThread)
    {
        WaitForSingleObject(m_hWorkThread, 5000);
        CloseHandle(m_hWorkThread);
    }
    if (m_hWorkEvent) CloseHandle(m_hWorkEvent);
    DeleteCriticalSection(&m_cs);
}

void CJobProcessor::Init(HWND hNotifyWnd)
{
    m_hNotifyWnd = hNotifyWnd;
    m_hWorkEvent  = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    m_hWorkThread = CreateThread(nullptr, 0, WorkThreadProc, this, 0, nullptr);
}

void CJobProcessor::OnStartJob(DWORD jobId, const wchar_t* docName, const wchar_t* userName)
{
    EnterCriticalSection(&m_cs);

    PrintJob job;
    job.jobId    = jobId;
    job.docName  = docName ? docName : L"(Unknown)";
    job.userName = userName ? userName : L"";
    job.status   = JobStatus::Receiving;
    GetLocalTime(&job.submitTime);

    // Create temp file for streaming spool data
    job.tempFilePath = CreateTempFile(jobId);
    if (!job.tempFilePath.empty())
    {
        job.hTempFile = CreateFileW(job.tempFilePath.c_str(),
                                    GENERIC_WRITE, 0, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    }

    m_jobs[jobId] = std::move(job);
    LeaveCriticalSection(&m_cs);

    NotifyUI(WM_MIT_JOB_ADDED, jobId, (DWORD)JobStatus::Receiving);
}

void CJobProcessor::OnDataChunk(DWORD jobId, DWORD offset, const BYTE* pData, DWORD cbData)
{
    EnterCriticalSection(&m_cs);
    auto it = m_jobs.find(jobId);
    if (it != m_jobs.end())
    {
        PrintJob& job = it->second;
        if (job.hTempFile != INVALID_HANDLE_VALUE)
        {
            DWORD written = 0;
            WriteFile(job.hTempFile, pData, cbData, &written, nullptr);
            job.totalBytes += written;
        }
    }
    LeaveCriticalSection(&m_cs);
    (void)offset;
}

void CJobProcessor::OnEndJob(DWORD jobId)
{
    EnterCriticalSection(&m_cs);
    auto it = m_jobs.find(jobId);
    if (it != m_jobs.end())
    {
        PrintJob& job = it->second;
        if (job.hTempFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(job.hTempFile);
            job.hTempFile = INVALID_HANDLE_VALUE;
        }
        job.status = JobStatus::ReadyToPrint;
    }
    LeaveCriticalSection(&m_cs);

    NotifyUI(WM_MIT_JOB_STATUS, jobId, (DWORD)JobStatus::ReadyToPrint);
    SetEvent(m_hWorkEvent);
}

void CJobProcessor::CancelJob(DWORD jobId)
{
    EnterCriticalSection(&m_cs);
    auto it = m_jobs.find(jobId);
    if (it != m_jobs.end())
    {
        PrintJob& job = it->second;
        if (job.hTempFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(job.hTempFile);
            job.hTempFile = INVALID_HANDLE_VALUE;
        }
        if (!job.tempFilePath.empty())
            DeleteFileW(job.tempFilePath.c_str());
        job.status = JobStatus::Cancelled;
    }
    LeaveCriticalSection(&m_cs);
    NotifyUI(WM_MIT_JOB_STATUS, jobId, (DWORD)JobStatus::Cancelled);
}

std::map<DWORD, PrintJob> CJobProcessor::GetJobsCopy()
{
    EnterCriticalSection(&m_cs);
    auto copy = m_jobs;
    LeaveCriticalSection(&m_cs);
    return copy;
}

const PrintJob* CJobProcessor::FindJob(DWORD jobId)
{
    // Caller must hold m_cs or accept potential race for display only
    auto it = m_jobs.find(jobId);
    return (it != m_jobs.end()) ? &it->second : nullptr;
}

// ---------------------------------------------------------------
// Worker thread — picks up ReadyToPrint jobs sequentially
// ---------------------------------------------------------------
DWORD WINAPI CJobProcessor::WorkThreadProc(LPVOID pThis)
{
    ((CJobProcessor*)pThis)->WorkThread();
    return 0;
}

void CJobProcessor::WorkThread()
{
    while (!m_bStop)
    {
        WaitForSingleObject(m_hWorkEvent, INFINITE);
        if (m_bStop) break;

        // Find next ReadyToPrint job (FIFO by jobId)
        DWORD nextJobId = 0;
        EnterCriticalSection(&m_cs);
        for (auto& [id, job] : m_jobs)
        {
            if (job.status == JobStatus::ReadyToPrint)
            {
                nextJobId    = id;
                job.status   = JobStatus::Printing;
                break;
            }
        }
        LeaveCriticalSection(&m_cs);

        if (nextJobId == 0) continue;

        // Retrieve job data under lock
        std::wstring tempPath, docName;
        EnterCriticalSection(&m_cs);
        auto it = m_jobs.find(nextJobId);
        if (it != m_jobs.end())
        {
            tempPath = it->second.tempFilePath;
            docName  = it->second.docName;
        }
        LeaveCriticalSection(&m_cs);

        if (tempPath.empty()) continue;

        NotifyUI(WM_MIT_JOB_STATUS, nextJobId, (DWORD)JobStatus::Printing);

        // Load spool data from temp file
        HANDLE hFile = CreateFileW(tempPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
        BOOL printed = FALSE;
        if (hFile != INVALID_HANDLE_VALUE)
        {
            DWORD fileSize = GetFileSize(hFile, nullptr);
            if (fileSize > 0)
            {
                std::vector<BYTE> spoolData(fileSize);
                DWORD bytesRead = 0;
                if (ReadFile(hFile, spoolData.data(), fileSize, &bytesRead, nullptr) &&
                    bytesRead == fileSize)
                {
                    CPrinterSettings settings;
                    CPrinterSettingsStore::Load(settings);

                    CEmfRenderer renderer;
                    printed = renderer.RenderToRealPrinter(
                        settings.targetPrinter.c_str(),
                        docName.c_str(),
                        spoolData.data(),
                        fileSize,
                        settings);
                }
            }
            CloseHandle(hFile);
        }

        // Cleanup temp file
        DeleteFileW(tempPath.c_str());

        // Update status
        JobStatus finalStatus = printed ? JobStatus::Done : JobStatus::Failed;
        EnterCriticalSection(&m_cs);
        auto it2 = m_jobs.find(nextJobId);
        if (it2 != m_jobs.end())
            it2->second.status = finalStatus;
        LeaveCriticalSection(&m_cs);

        NotifyUI(WM_MIT_JOB_STATUS, nextJobId, (DWORD)finalStatus);

        // Check if more jobs are pending
        EnterCriticalSection(&m_cs);
        for (auto& [id, job] : m_jobs)
        {
            if (job.status == JobStatus::ReadyToPrint)
            {
                SetEvent(m_hWorkEvent);
                break;
            }
        }
        LeaveCriticalSection(&m_cs);
    }
}

std::wstring CJobProcessor::CreateTempFile(DWORD jobId)
{
    wchar_t tempDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempDir);

    wchar_t path[MAX_PATH] = {};
    _snwprintf_s(path, MAX_PATH, _TRUNCATE, L"%sMitJob_%08X.spl", tempDir, jobId);
    return path;
}

void CJobProcessor::NotifyUI(UINT msg, DWORD jobId, DWORD status)
{
    if (m_hNotifyWnd)
        PostMessageW(m_hNotifyWnd, msg, (WPARAM)jobId, (LPARAM)status);
}
