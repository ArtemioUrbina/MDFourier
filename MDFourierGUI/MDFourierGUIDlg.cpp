
// MDFourierGUIDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MDFourierGUI.h"
#include "MDFourierGUIDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMDFourierGUIDlg dialog
CMDFourierGUIDlg::CMDFourierGUIDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CMDFourierGUIDlg::IDD, pParent)
	, m_TargetFile(_T(""))
	, m_Reference(_T(""))
	, m_Output(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMDFourierGUIDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_TARGET_FILE, m_TargetFile);
	DDX_Text(pDX, IDC_REFERENCE_FILE, m_Reference);
	DDX_Text(pDX, IDC_OUTPUT, m_Output);
	DDX_Control(pDX, IDC_REFERENCE_FILE, m_ReferenceLbl);
	DDX_Control(pDX, IDC_TARGET_FILE, m_TargetLbl);
	DDX_Control(pDX, IDC_OUTPUT, m_OutputCtrl);
	DDX_Control(pDX, ID_OPENRESULTS, m_OpenResultsBttn);
	DDX_Control(pDX, IDOK, m_ExecuteBttn);
	DDX_Control(pDX, IDC_WINDOW, m_WindowTypeSelect);
	DDX_Control(pDX, IDC_CURVEADJUST, m_CurveAdjustSelect);
	DDX_Control(pDX, IDC_SELECT_REFERENCE_FILE, m_ReferenceFileBttn);
	DDX_Control(pDX, IDC_SELECT_REFERENCE_COMPARE, m_TargetFileBttn);
	DDX_Control(pDX, IDC_ALIGN, m_AlignFFTW);
}

BEGIN_MESSAGE_MAP(CMDFourierGUIDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_SELECT_REFERENCE_FILE, &CMDFourierGUIDlg::OnBnClickedSelectReferenceFile)
	ON_BN_CLICKED(IDC_SELECT_REFERENCE_COMPARE, &CMDFourierGUIDlg::OnBnClickedSelectReferenceCompare)
	ON_BN_CLICKED(IDOK, &CMDFourierGUIDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CMDFourierGUIDlg::OnBnClickedCancel)
	ON_WM_TIMER()
	ON_BN_CLICKED(ID_OPENRESULTS, &CMDFourierGUIDlg::OnBnClickedOpenresults)
	ON_BN_CLICKED(IDC_ABOUT, &CMDFourierGUIDlg::OnBnClickedAbout)
END_MESSAGE_MAP()


// CMDFourierGUIDlg message handlers

BOOL CMDFourierGUIDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	if(!CheckDependencies())
	{
		EndDialog(IDCANCEL);
		return FALSE;
	}

	FillComboBoxes();

	m_OpenResultsBttn.EnableWindow(FALSE);
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CMDFourierGUIDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CMDFourierGUIDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CMDFourierGUIDlg::OnBnClickedSelectReferenceFile()
{
	CString fileName;
	TCHAR szFilters[]= L"Audio Files (*.wav)|*.wav";
	CFileDialog dlgFile(TRUE, L"wav", L"*.wav", OFN_FILEMUSTEXIST, szFilters);
	wchar_t* p = NULL;

	p = fileName.GetBuffer(BUFFER_SIZE);
	OPENFILENAME& ofn = dlgFile.GetOFN();
	ofn.lpstrFile = p;
	ofn.nMaxFile = BUFFER_SIZE;

	if(dlgFile.DoModal() != IDOK)
	{
		fileName.ReleaseBuffer();
		return;
	}

	fileName.ReleaseBuffer();
	m_Reference = fileName;
	m_ReferenceLbl.SetWindowText(m_Reference); 
}


void CMDFourierGUIDlg::OnBnClickedSelectReferenceCompare()
{
	CString fileName;
	TCHAR szFilters[]= L"Audio Files (*.wav)|*.wav";
	CFileDialog dlgFile(TRUE, L"wav", L"*.wav", OFN_FILEMUSTEXIST, szFilters);
	wchar_t* p = NULL;

	p = fileName.GetBuffer(BUFFER_SIZE);
	OPENFILENAME& ofn = dlgFile.GetOFN();
	ofn.lpstrFile = p;
	ofn.nMaxFile = BUFFER_SIZE;

	if(dlgFile.DoModal() != IDOK)
	{
		fileName.ReleaseBuffer();
		return;
	}

	fileName.ReleaseBuffer();
	m_TargetFile = fileName;
	m_TargetLbl.SetWindowText(m_TargetFile); 
}


void CMDFourierGUIDlg::OnBnClickedOk()
{
	CString	command;
	char	window, adjust;

	UpdateWindow();

	m_OpenResultsBttn.EnableWindow(FALSE);

	if(!cDos.m_fDone)
	{
		MessageBox(L"MDFourier is running, please wait for results", L"Please wait");
		return;
	}

	if(!m_Reference.GetLength())
	{
		MessageBox(L"Please select a reference WAV file", L"Error");
		return;
	}
	if(!m_TargetFile.GetLength())
	{
		MessageBox(L"Please select a WAV file to compare", L"Error");
		return;
	}
	if(m_Reference == m_TargetFile)
	{
		MessageBox(L"Reference and compare file are the same.\nPlease select a different file.", L"Error");
		return;
	}

	window = GetSelectedCommandLineValue(WindowConvert, m_WindowTypeSelect, COUNT_WINDOWS);
	adjust = GetSelectedCommandLineValue(CurveConvert, m_CurveAdjustSelect, COUNT_CURVES);

	command.Format(L"mdfourier.exe -r \"%s\" -c \"%s\" -w %c -o %c", m_Reference, m_TargetFile, window, adjust);

	if(m_AlignFFTW.GetCheck() == BST_CHECKED)
		command += " -z";

	ManageWindows(FALSE);

	SetTimer(IDT_DOS, 250, 0);

	m_OutputCtrl.SetWindowText(L"");
	cDos.Start(command);
}


void CMDFourierGUIDlg::OnBnClickedCancel()
{
	if(cDos.m_fDone)
		CDialogEx::OnCancel();
	else
	{
		if(MessageBox(L"MDFourier is currently running.\nStop it and exit?", L"Error", MB_OKCANCEL) == IDOK)
		{
			cDos.KillNow();
			CDialogEx::OnCancel();
		}
	}
}


void CMDFourierGUIDlg::OnTimer(UINT_PTR nIDEvent)
{
	cDos.Lock();
	m_OutputCtrl.SetWindowText(cDos.m_Output);
	cDos.Release();
    if (cDos.m_fDone)
	{
		int		pos = 0;
		CString	searchFor = L"Results stored in ";

        KillTimer(IDT_DOS);
        ManageWindows(TRUE);

		// Check is we enable the results button
		pos = cDos.m_Output.Find(searchFor, 0);
		if(pos != -1)
		{
			TCHAR	pwd[MAX_PATH];

			GetCurrentDirectory(MAX_PATH, pwd);
			m_OpenResultsBttn.EnableWindow(TRUE);

			resultsFolder.Format(L"%s\\%s", pwd,
				cDos.m_Output.Right(cDos.m_Output.GetLength() - pos - searchFor.GetLength()));
			resultsFolder = resultsFolder.Left(resultsFolder.GetLength() - 2);
		}
    }
	CDialogEx::OnTimer(nIDEvent);
}


void CMDFourierGUIDlg::OnBnClickedOpenresults()
{
	PIDLIST_ABSOLUTE pidl;

	if (SUCCEEDED(SHParseDisplayName(CT2W(resultsFolder), 0, &pidl, 0, 0)))
	{
		ITEMIDLIST idNull = { 0 };
		LPCITEMIDLIST pidlNull[1] = { &idNull };
		SHOpenFolderAndSelectItems(pidl, 1, pidlNull, 0);
		ILFree(pidl);
	}
}

char CMDFourierGUIDlg::GetSelectedCommandLineValue(CommandLineArray *Data, CComboBox &Combo, int size)
{
	int selected = -1, i;

	selected = Combo.GetCurSel();
	for(i = 0; i < size; i++)
	{
		if(Data[i].indexCB == selected)
			return Data[i].valueMDF;
	}
	return '-';
}

void CMDFourierGUIDlg::InsertValueInCombo(CString Name, char value, CommandLineArray &Data, CComboBox &Combo)
{
	Data.Name = Name;
	Data.indexCB = Combo.AddString(Name);
	Data.valueMDF = value;
}

void CMDFourierGUIDlg::FillComboBoxes()
{
	InsertValueInCombo(L"None", 'n', WindowConvert[0], m_WindowTypeSelect);
	InsertValueInCombo(L"Tukey", 't', WindowConvert[1], m_WindowTypeSelect);
	InsertValueInCombo(L"Flattop", 'f', WindowConvert[2], m_WindowTypeSelect);
	InsertValueInCombo(L"Hann", 'h', WindowConvert[3], m_WindowTypeSelect);
	InsertValueInCombo(L"Hamming", 'm', WindowConvert[4], m_WindowTypeSelect);

	m_WindowTypeSelect.SetCurSel(WindowConvert[1].indexCB);

	InsertValueInCombo(L"None", '0', CurveConvert[0], m_CurveAdjustSelect);
	InsertValueInCombo(L"\u221A(dBFS)", '1', CurveConvert[1], m_CurveAdjustSelect);
	InsertValueInCombo(L"ibeta(3, 3)", '2', CurveConvert[2], m_CurveAdjustSelect);
	InsertValueInCombo(L"Linear", '3', CurveConvert[3], m_CurveAdjustSelect);
	InsertValueInCombo(L"dBFS\u00B2", '4', CurveConvert[4], m_CurveAdjustSelect);
	InsertValueInCombo(L"ibeta(16, 2)", '5', CurveConvert[5], m_CurveAdjustSelect);

	m_CurveAdjustSelect.SetCurSel(CurveConvert[3].indexCB);
}

int CMDFourierGUIDlg::CheckDependencies()
{
	FILE *file;
	errno_t err;
	CString	msg;
	TCHAR	pwd[MAX_PATH];

	err = _wfopen_s(&file, L"mdfourier.exe", L"r");
	if(err != 0)
	{
		GetCurrentDirectory(MAX_PATH, pwd);
		msg.Format(L"Please place mdfourier.exe in:\n %s", pwd);
		MessageBox(msg, L"Error mdfourier.exe not found");
		return FALSE;
	}
	fclose(file);

	err = _wfopen_s(&file, L"mdfblocks.mfn", L"r");
	if(err != 0)
	{
		GetCurrentDirectory(MAX_PATH, pwd);
		msg.Format(L"Please place mdfblocks.mfn in:\n %s", pwd);
		MessageBox(msg, L"Error mdfblocks.mfn not found");
		return FALSE;
	}
	fclose(file);

	return TRUE;
}

void CMDFourierGUIDlg::ManageWindows(BOOL Enable)
{
	m_ExecuteBttn.EnableWindow(Enable);
	m_ReferenceFileBttn.EnableWindow(Enable);
	m_TargetFileBttn.EnableWindow(Enable);
	m_WindowTypeSelect.EnableWindow(Enable);
	m_CurveAdjustSelect.EnableWindow(Enable);
	m_AlignFFTW.EnableWindow(Enable);
}


void CMDFourierGUIDlg::OnBnClickedAbout()
{
	if(MessageBox(L"MDFourier Front End\n\nArtemio Urbina 2019\nCode available under GPL\n\nhttp://junkerhq.net/MDFourier/\n\nOpen website?", L"About MDFourier", MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
	{
		ShellExecute(0, 0, L"http://junkerhq.net/MDFourier/", 0, 0 , SW_SHOW );
	}
}
