#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

class CTrayIcon
{
public:
    CTrayIcon();
    ~CTrayIcon();

    BOOL Install(HWND hWnd, UINT nIconID, UINT nTooltipStrID);
    void Remove();
    void SetIcon(UINT nIconID);
    void ShowBalloon(const wchar_t* pTitle, const wchar_t* pText, DWORD dwInfoFlags = NIIF_INFO);

private:
    NOTIFYICONDATAW m_nid  = {};
    BOOL            m_bInstalled = FALSE;
    UINT            m_uTaskbarCreatedMsg = 0;
};
