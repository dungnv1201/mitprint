#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "JobProcessor.h"

class CPipeServer
{
public:
    CPipeServer();
    ~CPipeServer();

    BOOL Start(HWND hNotifyWnd, CJobProcessor* pJobProc);
    void Stop();

private:
    HWND           m_hNotifyWnd = nullptr;
    CJobProcessor* m_pJobProc   = nullptr;
    HANDLE         m_hThread    = nullptr;
    HANDLE         m_hStopEvent = nullptr;
    volatile BOOL  m_bRunning   = FALSE;

    static DWORD WINAPI IoThreadProc(LPVOID pThis);
    void IoThread();

    HANDLE CreateServerPipe();
    void DispatchMessage(const BYTE* pBuf, DWORD cbBuf);
    static BOOL BuildPipeSecurityAttr(SECURITY_ATTRIBUTES* pSa, PSECURITY_DESCRIPTOR* ppSd);
};
