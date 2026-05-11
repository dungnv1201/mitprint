#pragma once
#include <afxwin.h>
#include <afxcmn.h>
#include "resource.h"
#include "PrinterSettingsStore.h"

class CSettingsDlg : public CDialog
{
    DECLARE_DYNAMIC(CSettingsDlg)
public:
    explicit CSettingsDlg(CWnd* pParent);

    enum { IDD = IDD_SETTINGS };

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;

    afx_msg void OnBtnApply();
    afx_msg void OnCheckDuplex();
    afx_msg void OnSelChangePrinter();

    DECLARE_MESSAGE_MAP()

private:
    CComboBox m_comboPrinter;
    CComboBox m_comboPaper;
    CEdit     m_editCopies;
    CSpinButtonCtrl m_spinCopies;
    CButton   m_checkDuplex;
    CComboBox m_comboDuplex;
    CComboBox m_comboOrient;

    CPrinterSettings m_settings;

    void PopulatePrinters();
    void PopulatePaperSizes(const wchar_t* printerName);
    void LoadCurrentSettings();
    void SaveCurrentSettings();
};
