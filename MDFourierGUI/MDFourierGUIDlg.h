
// MDFourierGUIDlg.h : header file
//

#pragma once
#include "afxwin.h"
#include "DOSExecute.h"

#define	IDT_DOS	1000

#define	COUNT_CURVES	6
#define COUNT_WINDOWS	5

typedef struct commandline_st {
	CString	Name;
	char	valueMDF;
	int		indexCB;
} CommandLineArray;

// CMDFourierGUIDlg dialog
class CMDFourierGUIDlg : public CDialogEx
{
// Construction
public:
	CMDFourierGUIDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_MDFOURIERGUI_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	CString m_TargetFile;
	CString m_Reference;
	CString m_Output;
	CString resultsFolder;

	CStatic m_ReferenceLbl;
	CStatic m_TargetLbl;
	CEdit	m_OutputCtrl;
	CButton m_OpenResultsBttn;
	CButton m_ExecuteBttn;
	CButton m_ReferenceFileBttn;
	CButton m_TargetFileBttn;

	CComboBox m_WindowTypeSelect;
	CComboBox m_CurveAdjustSelect;

	CDOSExecute	cDos;
	CommandLineArray WindowConvert[COUNT_WINDOWS];
	CommandLineArray CurveConvert[COUNT_CURVES];

	void FillComboBoxes();
	int CheckDependencies();
	void ManageWindows(BOOL Enable);
	void InsertValueInCombo(CString Name, char value, CommandLineArray &Data, CComboBox &Combo);
	char GetSelectedCommandLineValue(CommandLineArray *Data, CComboBox &Combo, int size);

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedSelectReferenceFile();
	afx_msg void OnBnClickedSelectReferenceCompare();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnBnClickedOpenresults();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedAbout();
};
