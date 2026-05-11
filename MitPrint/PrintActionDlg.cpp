#include <afxwin.h>
#include "PrintActionDlg.h"
#include "PrinterSettingsStore.h"
#include "resource.h"
#include "MitLog.h"

IMPLEMENT_DYNAMIC(CPrintActionDlg, CDialog)

BEGIN_MESSAGE_MAP(CPrintActionDlg, CDialog)
    ON_BN_CLICKED(IDC_BTN_SAVE_PDF,       OnBtnSavePdf)
    ON_BN_CLICKED(IDC_BTN_PRINT_REAL,     OnBtnPrintReal)
    ON_BN_CLICKED(IDC_BTN_CANCEL_ACTION,  OnBtnCancel)
END_MESSAGE_MAP()

CPrintActionDlg::CPrintActionDlg(CWnd* pParent, CJobProcessor* pJobProc, DWORD jobId)
    : CDialog(IDD_PRINT_ACTION, pParent)
    , m_pJobProc(pJobProc)
    , m_jobId(jobId)
{
}

void CPrintActionDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BOOL CPrintActionDlg::OnInitDialog()
{
    MitLog("PrintActionDlg::OnInitDialog jobId=%u", m_jobId);
    CDialog::OnInitDialog();

    // Fill in document info from job
    auto jobs = m_pJobProc->GetJobsCopy();
    auto it = jobs.find(m_jobId);
    if (it != jobs.end())
    {
        SetDlgItemTextW(IDC_STATIC_DOC_NAME, it->second.docName.c_str());

        CString info;
        info.Format(L"User: %s   |   Size: %u KB",
                    it->second.userName.c_str(),
                    (it->second.totalBytes + 1023) / 1024);
        SetDlgItemTextW(IDC_STATIC_DOC_INFO, info);
    }

    // Force window to top — tray apps are blocked by Windows from stealing focus
    // normally, so we use SetWindowPos + FlashWindow
    ::SetWindowPos(GetSafeHwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    ::AllowSetForegroundWindow(ASFW_ANY);
    SetForegroundWindow();
    FlashWindow(TRUE);

    return TRUE;
}

void CPrintActionDlg::OnBtnSavePdf()
{
    MitLog("PrintActionDlg::OnBtnSavePdf jobId=%u", m_jobId);
    // Ask user where to save
    wchar_t filePath[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = GetSafeHwnd();
    ofn.lpstrFilter  = L"PDF Document (*.pdf)\0*.pdf\0\0";
    ofn.lpstrDefExt  = L"pdf";
    ofn.lpstrFile    = filePath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle   = L"Save as PDF";

    // Suggest filename from document name
    auto jobs = m_pJobProc->GetJobsCopy();
    auto it = jobs.find(m_jobId);
    if (it != jobs.end())
    {
        wcsncpy_s(filePath, it->second.docName.c_str(), MAX_PATH - 1);
        // Strip invalid filename chars
        for (wchar_t* p = filePath; *p; ++p)
            if (*p == L'\\' || *p == L'/' || *p == L':' || *p == L'*' ||
                *p == L'?' || *p == L'"' || *p == L'<' || *p == L'>' || *p == L'|')
                *p = L'_';
    }

    if (!GetSaveFileNameW(&ofn)) return; // user cancelled file dialog

    m_pJobProc->SetJobAction(m_jobId, JobAction::SavePdf, filePath);
    EndDialog(IDOK);
}

void CPrintActionDlg::OnBtnPrintReal()
{
    MitLog("PrintActionDlg::OnBtnPrintReal jobId=%u", m_jobId);

    // Check if a target printer is configured
    CPrinterSettings settings;
    CPrinterSettingsStore::Load(settings);
    if (settings.targetPrinter.empty())
    {
        MitLog("PrintActionDlg::OnBtnPrintReal - no target printer configured!");
        MessageBoxW(
            L"No target printer configured.\n\n"
            L"Please right-click the MitPrint tray icon, choose Settings, "
            L"select a target printer, and click Apply.",
            L"MitPrint - No Printer", MB_OK | MB_ICONWARNING);
        return; // keep dialog open so user can choose a different action
    }

    m_pJobProc->SetJobAction(m_jobId, JobAction::PrintToReal);
    EndDialog(IDOK);
}

void CPrintActionDlg::OnBtnCancel()
{
    MitLog("PrintActionDlg::OnBtnCancel jobId=%u", m_jobId);
    m_pJobProc->SetJobAction(m_jobId, JobAction::Cancel);
    EndDialog(IDCANCEL);
}
