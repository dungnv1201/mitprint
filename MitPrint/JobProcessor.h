#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <map>
#include <vector>

enum class JobStatus
{
    Queued = 0,
    Receiving,
    ReadyToPrint,   // data complete, waiting for user action
    Printing,
    Done,
    Failed,
    Cancelled
};

enum class JobAction
{
    Pending = 0,
    SavePdf,
    PrintToReal,
    Cancel
};

struct PrintJob
{
    DWORD           jobId        = 0;
    std::wstring    docName;
    std::wstring    userName;
    JobStatus       status       = JobStatus::Queued;
    DWORD           totalBytes   = 0;
    SYSTEMTIME      submitTime   = {};
    std::wstring    tempFilePath;
    HANDLE          hTempFile    = INVALID_HANDLE_VALUE;

    // Set by action dialog before signaling hActionEvent
    JobAction       action       = JobAction::Pending;
    std::wstring    pdfOutputPath;
    HANDLE          hActionEvent = nullptr; // signaled when user picks action
};

class CJobProcessor
{
public:
    CJobProcessor();
    ~CJobProcessor();

    void Init(HWND hNotifyWnd);

    // Called by PipeServer thread
    void OnStartJob(DWORD jobId, const wchar_t* docName, const wchar_t* userName);
    void OnDataChunk(DWORD jobId, DWORD offset, const BYTE* pData, DWORD cbData);
    void OnEndJob(DWORD jobId);
    void CancelJob(DWORD jobId);

    // Called by UI
    std::map<DWORD, PrintJob> GetJobsCopy();
    const PrintJob* FindJob(DWORD jobId);

    // Called by action dialog to deliver user's choice
    void SetJobAction(DWORD jobId, JobAction action, const wchar_t* pdfPath = nullptr);

private:
    HWND              m_hNotifyWnd = nullptr;
    CRITICAL_SECTION  m_cs        = {};
    std::map<DWORD, PrintJob> m_jobs;
    HANDLE            m_hWorkEvent = nullptr;
    HANDLE            m_hWorkThread = nullptr;
    volatile BOOL     m_bStop     = FALSE;

    static DWORD WINAPI WorkThreadProc(LPVOID pThis);
    void WorkThread();
    void ProcessJob(PrintJob& job);
    std::wstring CreateTempFile(DWORD jobId);
    void NotifyUI(UINT msg, DWORD jobId, DWORD status);
};
