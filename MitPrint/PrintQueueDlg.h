#pragma once
#include <afxwin.h>
#include <afxcmn.h>
#include "resource.h"
#include "JobProcessor.h"

class CPrintQueueDlg : public CDialog
{
    DECLARE_DYNAMIC(CPrintQueueDlg)
public:
    CPrintQueueDlg(CWnd* pParent, CJobProcessor* pJobProc);

    enum { IDD = IDD_PRINT_QUEUE };

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;

    afx_msg void OnBtnCancelJob();
    afx_msg void OnBtnRefresh();
    afx_msg void OnTimer(UINT_PTR nIDEvent);

    DECLARE_MESSAGE_MAP()

private:
    CListCtrl      m_listJobs;
    CJobProcessor* m_pJobProc = nullptr;

    void PopulateJobList();
    static CString StatusToString(JobStatus s);
    static CString FormatTime(const SYSTEMTIME& st);
};
