#include <afxwin.h>
#include <afxcmn.h>
#include "SettingsDlg.h"
#include "resource.h"
#include <winspool.h>
#include <vector>

IMPLEMENT_DYNAMIC(CSettingsDlg, CDialog)

BEGIN_MESSAGE_MAP(CSettingsDlg, CDialog)
    ON_BN_CLICKED(IDC_BTN_APPLY,        OnBtnApply)
    ON_BN_CLICKED(IDC_CHECK_DUPLEX,     OnCheckDuplex)
    ON_CBN_SELCHANGE(IDC_COMBO_PRINTER, OnSelChangePrinter)
END_MESSAGE_MAP()

CSettingsDlg::CSettingsDlg(CWnd* pParent)
    : CDialog(IDD_SETTINGS, pParent)
{
}

void CSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMBO_PRINTER,  m_comboPrinter);
    DDX_Control(pDX, IDC_COMBO_PAPER,    m_comboPaper);
    DDX_Control(pDX, IDC_EDIT_COPIES,    m_editCopies);
    DDX_Control(pDX, IDC_SPIN_COPIES,    m_spinCopies);
    DDX_Control(pDX, IDC_CHECK_DUPLEX,   m_checkDuplex);
    DDX_Control(pDX, IDC_COMBO_DUPLEX,   m_comboDuplex);
    DDX_Control(pDX, IDC_COMBO_ORIENT,   m_comboOrient);
}

BOOL CSettingsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    m_spinCopies.SetRange(1, 999);

    // Orientation
    m_comboOrient.AddString(L"Dọc (Portrait)");
    m_comboOrient.AddString(L"Ngang (Landscape)");

    // Duplex options
    m_comboDuplex.AddString(L"Cạnh dài (Long Edge)");
    m_comboDuplex.AddString(L"Cạnh ngắn (Short Edge)");

    CPrinterSettingsStore::Load(m_settings);
    PopulatePrinters();
    LoadCurrentSettings();

    return TRUE;
}

void CSettingsDlg::PopulatePrinters()
{
    m_comboPrinter.ResetContent();

    DWORD cbNeeded = 0, nReturned = 0;
    EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
                  nullptr, 2, nullptr, 0, &cbNeeded, &nReturned);

    if (cbNeeded == 0) return;

    std::vector<BYTE> buf(cbNeeded);
    if (!EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
                       nullptr, 2, buf.data(), cbNeeded, &cbNeeded, &nReturned))
        return;

    PRINTER_INFO_2W* pi = (PRINTER_INFO_2W*)buf.data();
    for (DWORD i = 0; i < nReturned; ++i)
    {
        if (pi[i].pPrinterName)
            m_comboPrinter.AddString(pi[i].pPrinterName);
    }
}

void CSettingsDlg::PopulatePaperSizes(const wchar_t* printerName)
{
    m_comboPaper.ResetContent();
    if (!printerName || !printerName[0]) return;

    // Get paper names
    int nPapers = (int)DeviceCapabilitiesW(printerName, nullptr,
                                            DC_PAPERNAMES, nullptr, nullptr);
    if (nPapers <= 0) return;

    std::vector<wchar_t> names(nPapers * 64);
    std::vector<WORD>    ids(nPapers);

    DeviceCapabilitiesW(printerName, nullptr, DC_PAPERNAMES, names.data(), nullptr);
    DeviceCapabilitiesW(printerName, nullptr, DC_PAPERS, (wchar_t*)ids.data(), nullptr);

    for (int i = 0; i < nPapers; ++i)
    {
        const wchar_t* pName = names.data() + i * 64;
        int idx = m_comboPaper.AddString(pName);
        m_comboPaper.SetItemData(idx, ids[i]);
    }
}

void CSettingsDlg::LoadCurrentSettings()
{
    // Select target printer
    int printerIdx = m_comboPrinter.FindStringExact(-1, m_settings.targetPrinter.c_str());
    m_comboPrinter.SetCurSel(printerIdx >= 0 ? printerIdx : 0);

    // Populate paper sizes for selected printer
    CString sel;
    m_comboPrinter.GetWindowTextW(sel);
    PopulatePaperSizes(sel);

    // Select paper
    for (int i = 0; i < m_comboPaper.GetCount(); ++i)
    {
        if ((DWORD)m_comboPaper.GetItemData(i) == m_settings.paperSize)
        {
            m_comboPaper.SetCurSel(i);
            break;
        }
    }

    // Copies
    CString copies;
    copies.Format(L"%d", m_settings.copies);
    m_editCopies.SetWindowTextW(copies);
    m_spinCopies.SetPos(m_settings.copies);

    // Duplex
    BOOL bDuplex = (m_settings.duplex != DMDUP_SIMPLEX);
    m_checkDuplex.SetCheck(bDuplex ? BST_CHECKED : BST_UNCHECKED);
    m_comboDuplex.EnableWindow(bDuplex);
    m_comboDuplex.SetCurSel(m_settings.duplex == DMDUP_HORIZONTAL ? 1 : 0);

    // Orientation
    m_comboOrient.SetCurSel(m_settings.orientation == DMORIENT_LANDSCAPE ? 1 : 0);
}

void CSettingsDlg::OnSelChangePrinter()
{
    CString sel;
    m_comboPrinter.GetWindowTextW(sel);
    PopulatePaperSizes(sel);
    if (m_comboPaper.GetCount() > 0)
        m_comboPaper.SetCurSel(0);
}

void CSettingsDlg::OnCheckDuplex()
{
    BOOL bDuplex = (m_checkDuplex.GetCheck() == BST_CHECKED);
    m_comboDuplex.EnableWindow(bDuplex);
}

void CSettingsDlg::OnBtnApply()
{
    SaveCurrentSettings();
    MessageBoxW(L"Cài đặt đã được lưu.", L"MitPrint", MB_OK | MB_ICONINFORMATION);
}

void CSettingsDlg::SaveCurrentSettings()
{
    CString sel;
    m_comboPrinter.GetWindowTextW(sel);
    m_settings.targetPrinter = sel.GetString();

    int paperIdx = m_comboPaper.GetCurSel();
    if (paperIdx >= 0)
        m_settings.paperSize = (DWORD)m_comboPaper.GetItemData(paperIdx);

    CString copies;
    m_editCopies.GetWindowTextW(copies);
    m_settings.copies = (short)_wtoi(copies);
    if (m_settings.copies < 1) m_settings.copies = 1;

    if (m_checkDuplex.GetCheck() == BST_CHECKED)
        m_settings.duplex = (m_comboDuplex.GetCurSel() == 1) ? DMDUP_HORIZONTAL : DMDUP_VERTICAL;
    else
        m_settings.duplex = DMDUP_SIMPLEX;

    m_settings.orientation = (m_comboOrient.GetCurSel() == 1)
                             ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;

    CPrinterSettingsStore::Save(m_settings);
}
