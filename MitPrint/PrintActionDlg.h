#pragma once
#include <afxwin.h>
#include "JobProcessor.h"
#include "resource.h"

class CPrintActionDlg : public CDialog
{
    DECLARE_DYNAMIC(CPrintActionDlg)
public:
    CPrintActionDlg(CWnd* pParent, CJobProcessor* pJobProc, DWORD jobId);

protected:
    BOOL OnInitDialog() override;
    void DoDataExchange(CDataExchange* pDX) override;
    void OnBtnSavePdf();
    void OnBtnPrintReal();
    void OnBtnCancel();
    DECLARE_MESSAGE_MAP()

private:
    CJobProcessor* m_pJobProc;
    DWORD          m_jobId;
};
