#include <afxwin.h>
#include "TrayIcon.h"
#include "resource.h"
#include "mit_guids.h"

CTrayIcon::CTrayIcon()
{
    m_uTaskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
}

CTrayIcon::~CTrayIcon()
{
    Remove();
}

BOOL CTrayIcon::Install(HWND hWnd, UINT nIconID, UINT nTooltipStrID)
{
    m_nid.cbSize           = sizeof(m_nid);
    m_nid.hWnd             = hWnd;
    m_nid.uID              = MIT_TRAY_ICON_ID;
    m_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    m_nid.uCallbackMessage = WM_MIT_TRAY;
    m_nid.hIcon            = (HICON)LoadImageW(AfxGetInstanceHandle(),
                                                MAKEINTRESOURCEW(nIconID),
                                                IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

    wchar_t tip[128] = L"MitPrint - Virtual Printer";
    LoadStringW(AfxGetInstanceHandle(), nTooltipStrID, tip, 128);
    lstrcpynW(m_nid.szTip, tip, sizeof(m_nid.szTip) / sizeof(wchar_t) - 1);

    m_bInstalled = Shell_NotifyIconW(NIM_ADD, &m_nid);

    // Set version for balloon support
    m_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &m_nid);

    return m_bInstalled;
}

void CTrayIcon::Remove()
{
    if (m_bInstalled)
    {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_bInstalled = FALSE;
        if (m_nid.hIcon)
        {
            DestroyIcon(m_nid.hIcon);
            m_nid.hIcon = nullptr;
        }
    }
}

void CTrayIcon::SetIcon(UINT nIconID)
{
    if (!m_bInstalled) return;
    HICON hOld = m_nid.hIcon;
    m_nid.hIcon = (HICON)LoadImageW(AfxGetInstanceHandle(),
                                    MAKEINTRESOURCEW(nIconID),
                                    IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    m_nid.uFlags = NIF_ICON;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    if (hOld) DestroyIcon(hOld);
}

void CTrayIcon::ShowBalloon(const wchar_t* pTitle, const wchar_t* pText, DWORD dwInfoFlags)
{
    if (!m_bInstalled) return;
    m_nid.uFlags = NIF_INFO;
    lstrcpynW(m_nid.szInfoTitle, pTitle,
              sizeof(m_nid.szInfoTitle) / sizeof(wchar_t) - 1);
    lstrcpynW(m_nid.szInfo, pText,
              sizeof(m_nid.szInfo) / sizeof(wchar_t) - 1);
    m_nid.dwInfoFlags = dwInfoFlags;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}
