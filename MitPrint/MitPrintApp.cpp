#include <afxwin.h>
#include <afxext.h>
#include "MitPrintApp.h"
#include "JobProcessor.h"
#include "protocol.h"
#include "PrintQueueDlg.h"
#include "PrintActionDlg.h"
#include "SettingsDlg.h"
#include "resource.h"
#include "mit_guids.h"
#include "MitLog.h"

BEGIN_MESSAGE_MAP(CMitPrintApp, CWinApp)
END_MESSAGE_MAP()

CMitPrintApp::CMitPrintApp()
{
}

CMitPrintApp::~CMitPrintApp()
{
}

BOOL CMitPrintApp::InitInstance()
{
    CWinApp::InitInstance();
    MitLog("MitPrint starting (pid=%u)", GetCurrentProcessId());

    // Enforce single instance
    m_hMutex = CreateMutexW(nullptr, TRUE, MIT_MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(m_hMutex);
        m_hMutex = nullptr;
        return FALSE;
    }

    // Enable common controls (ListView, etc.)
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    if (!CreateMsgWindow())
        return FALSE;

    // Initialize components
    m_jobProc.Init(m_hMsgWnd);
    m_pipeServer.Start(m_hMsgWnd, &m_jobProc);
    m_trayIcon.Install(m_hMsgWnd, IDI_TRAY, IDS_TRAY_TOOLTIP);

    return TRUE;
}

int CMitPrintApp::ExitInstance()
{
    m_pipeServer.Stop();
    m_trayIcon.Remove();

    if (m_hMsgWnd)
    {
        DestroyWindow(m_hMsgWnd);
        m_hMsgWnd = nullptr;
    }

    if (m_hMutex)
    {
        ReleaseMutex(m_hMutex);
        CloseHandle(m_hMutex);
        m_hMutex = nullptr;
    }

    return CWinApp::ExitInstance();
}

void CMitPrintApp::RegisterMsgWindowClass()
{
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = MsgWndProc;
    wc.hInstance     = AfxGetInstanceHandle();
    wc.lpszClassName = MIT_WINDOW_CLASS;
    RegisterClassExW(&wc);
}

BOOL CMitPrintApp::CreateMsgWindow()
{
    RegisterMsgWindowClass();
    m_hMsgWnd = CreateWindowExW(0, MIT_WINDOW_CLASS, MIT_APPNAME, 0,
                                 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                 AfxGetInstanceHandle(), this);
    return m_hMsgWnd != nullptr;
}

int CMitPrintApp::Run()
{
    // MFC's CWinApp::Run() calls PostQuitMessage when m_pMainWnd == NULL.
    // As a tray-only app we have no visible window, so run a bare message loop.
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK CMitPrintApp::MsgWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    CMitPrintApp* pApp = (CMitPrintApp*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return 0;
    }

    case WM_MIT_TRAY:
    {
        if (!pApp) return 0;
        switch (LOWORD(lp))
        {
        case WM_LBUTTONDBLCLK:
        case WM_MIT_SHOW_QUEUE:
        {
            CPrintQueueDlg dlg(nullptr, &pApp->m_jobProc);
            dlg.DoModal();
            break;
        }
        case WM_RBUTTONUP:
            pApp->ShowContextMenu(hwnd);
            break;
        }
        return 0;
    }

    case WM_MIT_JOB_ACTION:
        // Show action dialog: Save PDF / Print to real printer / Cancel
        MitLog("WM_MIT_JOB_ACTION: jobId=%u pApp=%p", (DWORD)wp, pApp);
        if (pApp)
        {
            // Notify user via tray balloon first (dialog may appear behind other windows)
            pApp->m_trayIcon.ShowBalloon(L"MitPrint", L"New print job — click to choose action", NIIF_INFO);
            CPrintActionDlg dlg(nullptr, &pApp->m_jobProc, (DWORD)wp);
            int modalRet = dlg.DoModal();
            MitLog("WM_MIT_JOB_ACTION: DoModal returned %d (jobId=%u)", modalRet, (DWORD)wp);
            if (modalRet == -1)
            {
                // Dialog creation failed — cancel the job
                MitLog("WM_MIT_JOB_ACTION: dialog creation FAILED, LastError=%u", GetLastError());
                pApp->m_jobProc.SetJobAction((DWORD)wp, JobAction::Cancel);
            }
        }
        return 0;

    case WM_MIT_JOB_ADDED:
    case WM_MIT_JOB_STATUS:
        if (pApp && lp == (DWORD)JobStatus::Done)
            pApp->m_trayIcon.ShowBalloon(L"MitPrint", L"Print job completed", NIIF_INFO);
        return 0;

    case WM_MIT_SHOW_SETTINGS:
        if (pApp)
        {
            CSettingsDlg dlg(nullptr);
            dlg.DoModal();
        }
        return 0;

    case WM_COMMAND:
        if (!pApp) return 0;
        switch (LOWORD(wp))
        {
        case IDM_OPEN_QUEUE:
        {
            CPrintQueueDlg dlg(nullptr, &pApp->m_jobProc);
            dlg.DoModal();
            break;
        }
        case IDM_SETTINGS:
        {
            CSettingsDlg dlg(nullptr);
            dlg.DoModal();
            break;
        }
        case IDM_EXIT:
            PostQuitMessage(0);
            break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void CMitPrintApp::ShowContextMenu(HWND hwnd)
{
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_OPEN_QUEUE,  L"Open Print Queue");
    AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS,    L"Settings...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0,            nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT,        L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);
}
