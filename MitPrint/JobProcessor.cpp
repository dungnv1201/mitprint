#include <afxwin.h>
#include "JobProcessor.h"
#include "EmfRenderer.h"
#include "PrinterSettingsStore.h"
#include "mit_guids.h"
#include "MitLog.h"

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
        // Create event so worker thread can wait for user's action choice
        job.hActionEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        job.status = JobStatus::ReadyToPrint;
    }
    LeaveCriticalSection(&m_cs);

    // Ask UI to show the print action dialog; worker waits on hActionEvent
    MitLog("OnEndJob: jobId=%u posting WM_MIT_JOB_ACTION to hwnd=%p", jobId, m_hNotifyWnd);
    NotifyUI(WM_MIT_JOB_ACTION, jobId, 0);
    SetEvent(m_hWorkEvent); // wake worker so it starts waiting
}

void CJobProcessor::SetJobAction(DWORD jobId, JobAction action, const wchar_t* pdfPath)
{
    HANDLE hEvt = nullptr;
    EnterCriticalSection(&m_cs);
    auto it = m_jobs.find(jobId);
    if (it != m_jobs.end())
    {
        it->second.action = action;
        if (pdfPath) it->second.pdfOutputPath = pdfPath;
        hEvt = it->second.hActionEvent;
    }
    LeaveCriticalSection(&m_cs);
    if (hEvt) SetEvent(hEvt);
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

        MitLog("WorkThread: picked jobId=%u, waiting for action...", nextJobId);

        // Wait for user to choose action via the action dialog (30s timeout)
        HANDLE hActionEvt = nullptr;
        {
            EnterCriticalSection(&m_cs);
            auto it = m_jobs.find(nextJobId);
            if (it != m_jobs.end()) hActionEvt = it->second.hActionEvent;
            LeaveCriticalSection(&m_cs);
        }
        MitLog("WorkThread: jobId=%u hActionEvt=%p", nextJobId, hActionEvt);
        if (hActionEvt)
        {
            DWORD waitRet = WaitForSingleObject(hActionEvt, 30000); // 30s timeout → auto-cancel
            MitLog("WorkThread: jobId=%u wait result=%u (0=signaled,258=timeout)", nextJobId, waitRet);
        }

        // Retrieve action + job data
        JobAction   chosenAction = JobAction::Cancel;
        std::wstring tempPath, docName, pdfPath;
        {
            EnterCriticalSection(&m_cs);
            auto it = m_jobs.find(nextJobId);
            if (it != m_jobs.end())
            {
                chosenAction = it->second.action;
                tempPath     = it->second.tempFilePath;
                docName      = it->second.docName;
                pdfPath      = it->second.pdfOutputPath;
                if (hActionEvt)
                {
                    CloseHandle(hActionEvt);
                    it->second.hActionEvent = nullptr;
                }
            }
            LeaveCriticalSection(&m_cs);
        }

        MitLog("WorkThread: jobId=%u chosenAction=%d tempPath=%s",
               nextJobId, (int)chosenAction, tempPath.empty() ? "EMPTY" : "OK");

        if (chosenAction == JobAction::Cancel ||
            chosenAction == JobAction::Pending || // timeout — treat as cancel
            tempPath.empty())
        {
            DeleteFileW(tempPath.c_str());
            EnterCriticalSection(&m_cs);
            auto it2 = m_jobs.find(nextJobId);
            if (it2 != m_jobs.end()) it2->second.status = JobStatus::Cancelled;
            LeaveCriticalSection(&m_cs);
            NotifyUI(WM_MIT_JOB_STATUS, nextJobId, (DWORD)JobStatus::Cancelled);
            continue;
        }

        // Mark as Printing
        {
            EnterCriticalSection(&m_cs);
            auto it = m_jobs.find(nextJobId);
            if (it != m_jobs.end()) it->second.status = JobStatus::Printing;
            LeaveCriticalSection(&m_cs);
        }
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
                    CEmfRenderer renderer;
                    if (chosenAction == JobAction::SavePdf)
                    {
                        printed = renderer.RenderToPdf(
                            docName.c_str(),
                            spoolData.data(), fileSize,
                            pdfPath.c_str());
                    }
                    else // PrintToReal
                    {
                        CPrinterSettings settings;
                        CPrinterSettingsStore::Load(settings);
                        printed = renderer.RenderToRealPrinter(
                            settings.targetPrinter.c_str(),
                            docName.c_str(),
                            spoolData.data(), fileSize,
                            settings);
                    }
                }
            }
            CloseHandle(hFile);
        }

        // Cleanup temp file
        DeleteFileW(tempPath.c_str());

        MitLog("WorkThread: jobId=%u render result=%d", nextJobId, (int)printed);

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
