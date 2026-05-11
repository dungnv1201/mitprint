#include <afxwin.h>
#include <afxcmn.h>
#include "PrintQueueDlg.h"
#include "resource.h"

IMPLEMENT_DYNAMIC(CPrintQueueDlg, CDialog)

BEGIN_MESSAGE_MAP(CPrintQueueDlg, CDialog)
    ON_BN_CLICKED(IDC_BTN_CANCEL_JOB, OnBtnCancelJob)
    ON_BN_CLICKED(IDC_BTN_REFRESH,    OnBtnRefresh)
    ON_WM_TIMER()
END_MESSAGE_MAP()

CPrintQueueDlg::CPrintQueueDlg(CWnd* pParent, CJobProcessor* pJobProc)
    : CDialog(IDD_PRINT_QUEUE, pParent)
    , m_pJobProc(pJobProc)
{
}

void CPrintQueueDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_JOBS, m_listJobs);
}

BOOL CPrintQueueDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Setup columns
    m_listJobs.InsertColumn(0, L"#",         LVCFMT_RIGHT,  40);
    m_listJobs.InsertColumn(1, L"Document",  LVCFMT_LEFT,  180);
    m_listJobs.InsertColumn(2, L"User",      LVCFMT_LEFT,   90);
    m_listJobs.InsertColumn(3, L"Status",    LVCFMT_LEFT,   90);
    m_listJobs.InsertColumn(4, L"Time",      LVCFMT_LEFT,   90);

    m_listJobs.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    PopulateJobList();

    // Auto-refresh every 2 seconds
    SetTimer(1, 2000, nullptr);
    return TRUE;
}

void CPrintQueueDlg::OnBtnCancelJob()
{
    if (!m_pJobProc) return;
    int sel = m_listJobs.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0) return;

    DWORD jobId = (DWORD)m_listJobs.GetItemData(sel);
    m_pJobProc->CancelJob(jobId);
    PopulateJobList();
}

void CPrintQueueDlg::OnBtnRefresh()
{
    PopulateJobList();
}

void CPrintQueueDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1)
        PopulateJobList();
    CDialog::OnTimer(nIDEvent);
}

void CPrintQueueDlg::PopulateJobList()
{
    if (!m_pJobProc) return;

    m_listJobs.DeleteAllItems();
    auto jobs = m_pJobProc->GetJobsCopy();

    int row = 0;
    for (auto& [id, job] : jobs)
    {
        CString num;
        num.Format(L"%u", id);
        m_listJobs.InsertItem(row, num);
        m_listJobs.SetItemText(row, 1, job.docName.c_str());
        m_listJobs.SetItemText(row, 2, job.userName.c_str());
        m_listJobs.SetItemText(row, 3, StatusToString(job.status));
        m_listJobs.SetItemText(row, 4, FormatTime(job.submitTime));
        m_listJobs.SetItemData(row, (DWORD_PTR)id);
        ++row;
    }
}

CString CPrintQueueDlg::StatusToString(JobStatus s)
{
    switch (s)
    {
    case JobStatus::Queued:        return L"Queued";
    case JobStatus::Receiving:     return L"Receiving";
    case JobStatus::ReadyToPrint:  return L"Waiting";
    case JobStatus::Printing:      return L"Printing";
    case JobStatus::Done:          return L"Done";
    case JobStatus::Failed:        return L"Failed";
    case JobStatus::Cancelled:     return L"Cancelled";
    default:                       return L"?";
    }
}

CString CPrintQueueDlg::FormatTime(const SYSTEMTIME& st)
{
    CString s;
    s.Format(L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    return s;
}
