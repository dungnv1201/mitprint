#pragma once
#include <afxwin.h>
#include "TrayIcon.h"
#include "PipeServer.h"
#include "JobProcessor.h"

class CMitPrintApp : public CWinApp
{
public:
    CMitPrintApp();
    ~CMitPrintApp() override;

    BOOL InitInstance() override;
    int  ExitInstance() override;
    int  Run() override;

    // Message-only window procedure
    static LRESULT CALLBACK MsgWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
    HANDLE        m_hMutex    = nullptr;
    HWND          m_hMsgWnd   = nullptr;
    CTrayIcon     m_trayIcon;
    CPipeServer   m_pipeServer;
    CJobProcessor m_jobProc;

    BOOL CreateMsgWindow();
    void RegisterMsgWindowClass();
    void ShowContextMenu(HWND hwnd);

    DECLARE_MESSAGE_MAP()
};
