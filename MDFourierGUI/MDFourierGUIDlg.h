
// MDFourierGUIDlg.h : header file
//

#pragma once
#include "afxwin.h"
#include "DOSExecute.h"

#define MDFVERSION			L"1.13"
#define	IDT_DOS				1000

#define	COUNT_CURVES		6
#define COUNT_WINDOWS		5
#define	COUNT_PROFILES		255
#define COUNT_SYNCTYPE		10
#define COUNT_RESOLUTION	8

typedef struct commandline_st {
	CString	Name;
	CString	valueMDF;
} CommandLineArray;

// CMDFourierGUIDlg dialog
class CMDFourierGUIDlg : public CDialogEx
{
// Construction
public:
	CMDFourierGUIDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_MDFOURIERGUI_DIALOG };

	LRESULT  OnDropFiles(WPARAM wParam,LPARAM lParam);

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

// Implementation
protected:
	HICON m_hIcon;

	CString m_ReferenceFile;
	CString m_ComparisonFile;
	CString m_OutputText;
	CString m_ResultsFolderText;

	CStatic m_ReferenceFileLbl;
	CStatic m_ComparisonLbl;
	CEdit	m_OutputTextCtrl;
	CEdit	m_ExtraParamsEditBox;

	CButton m_OpenResultsBttn;
	CButton m_ExecuteBttn;
	CButton m_CloseBttn;
	CButton m_ReferenceFileFileBttn;
	CButton m_ComparisonFileBttn;
	CButton m_MDWaveBttn;
	CButton m_SwapBttn;

	CButton m_ExtraDataCheckBox;
	CButton m_AlignFFTWCheckBox;
	CButton m_AveragePlotCheckBox;
	CButton m_VerboseLogCheckBox;
	CButton m_EnableExtraCommandCheckBox;
	CButton m_DifferencesCheckBox;
	CButton m_MissingExtraCheckBox;
	CButton m_SpectrogramsCheckBox;
	CButton m_NoiseFloorCheckBox;
	CButton m_TimeSpectrogramCheckBox;
	CButton m_FullResTimeSpectrCheckBox;
	CButton m_WaveFormCheckBox;
	CButton m_PhaseCheckBox;

	CComboBox m_RefSync;
	CComboBox m_ComSync;
	CComboBox m_Resolution;

	CComboBox m_WindowTypeSelect;
	CComboBox m_CurveAdjustSelect;
	CComboBox m_Profiles;

	CDOSExecute			cDos;
	CommandLineArray	WindowConvert[COUNT_WINDOWS];
	CommandLineArray	CurveConvert[COUNT_CURVES];
	CommandLineArray	Profiles[COUNT_PROFILES];
	CommandLineArray	SyncType[COUNT_SYNCTYPE];
	CommandLineArray	Resolutions[COUNT_RESOLUTION];

	CString cmdWindowText;
	CString MDFVersion;
	CString	ProfileVersion;
	CString listName;
	CString	*elements;
	CString	baseWintext;
	CString wintextProfile;
	CString multiWarnings;
	CString multiErrors;
	int		elementCount;
	int		elementPos;
	bool	mdwave;
	bool	killingDOS;
	int		DosWaitCount;
	int		syncTypes;

	int Monitor_vert;
	int Monitor_horz;

	void ReadAndDisplayResults(CString &newText);
	void CheckForDifferenceplots(CString ntext);
	void FillComboBoxes();
	int CheckDependencies();
	void ManageWindows(BOOL Enable);
	void InsertValueInCombo(CString Name, CString value, CommandLineArray &Data, CComboBox &Combo);
	int LoadProfile(CString FullFileName, CString &Name, CString &Version, CString &Error, CommandLineArray	ProfileSyncTypes[], int ArraySize, int &syncCount);
	CString GetSelectedCommandLineValue(CComboBox &Combo, int size);
	void ExecuteCommand(CString Compare);
	void CheckPlotSelection(CButton &clicked);
	int FindProfiles(CString sPath, CString pattern);
	bool VerifyFileExtension(CString filename, int type);
	void ClearResults() { m_OpenResultsBttn.EnableWindow(FALSE); m_OutputTextCtrl.SetWindowText(L""); m_ResultsFolderText = L""; };
	void ChangeWindowText(CString data = L"");

	void ReduceWindowSizeIfLowRes();
	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnCbnDropdownProfile();
	afx_msg void OnBnClickedSelectReferenceFile();
	afx_msg void OnBnClickedSelectReferenceCompare();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnBnClickedOpenresults();
	afx_msg void OnBnClickedAbout();
	afx_msg void OnBnClickedEnableextra();
	afx_msg void OnBnClickedDifferences();
	afx_msg void OnBnClickedMissing();
	afx_msg void OnBnClickedSpectrogram();
	afx_msg void OnBnClickedAverage();
	afx_msg void OnBnClickedNoisefloor();
	afx_msg void OnBnClickedMdwave();
	afx_msg void OnBnClickedSwap();
	afx_msg void OnBnClickedTimesp();
	afx_msg void OnBnClickedPlotTd();
	afx_msg void OnBnClickedPhase();
	afx_msg void OnCbnSelendokProfile();
	afx_msg void OnCbnSelendcancelProfile();
};
