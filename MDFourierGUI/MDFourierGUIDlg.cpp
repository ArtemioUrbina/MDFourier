
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
	, m_ComparisonFile(_T(""))
	, m_Reference(_T(""))
	, m_Output(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	elementCount = 0;
	elements = NULL;
	elementPos = 0;
}

void CMDFourierGUIDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_REFERENCE_FILE, m_ReferenceLbl);
	DDX_Control(pDX, IDC_COMPARISON_FILE, m_ComparisonLbl);
	DDX_Control(pDX, IDC_OUTPUT, m_OutputCtrl);
	DDX_Control(pDX, ID_OPENRESULTS, m_OpenResultsBttn);
	DDX_Control(pDX, IDOK, m_ExecuteBttn);
	DDX_Control(pDX, IDC_WINDOW, m_WindowTypeSelect);
	DDX_Control(pDX, IDC_CURVEADJUST, m_CurveAdjustSelect);
	DDX_Control(pDX, IDC_SELECT_REFERENCE_FILE, m_ReferenceFileBttn);
	DDX_Control(pDX, IDC_SELECT_REFERENCE_COMPARE, m_ComparisonFileBttn);
	DDX_Control(pDX, IDC_ALIGN, m_AlignFFTW);
	DDX_Control(pDX, IDC_AVERAGE, m_AveragePlot_Bttn);
	DDX_Control(pDX, IDC_VERBOSE, m_VerboseLog_Bttn);
	DDX_Control(pDX, IDC_EXTRACLPARAMS, m_ExtraParamsEditBox);
	DDX_Control(pDX, IDC_ENABLEEXTRA, m_EnableExtraBttn);
	DDX_Control(pDX, IDC_DIFFERENCES, m_DiffBttn);
	DDX_Control(pDX, IDC_MISSING, m_MissBttn);
	DDX_Control(pDX, IDC_SPECTROGRAM, m_SpectrBttn);
	DDX_Control(pDX, IDC_PROFILE, m_Profiles);
	DDX_Control(pDX, IDC_NOISEFLOOR, m_NoiseFloor);
	DDX_Control(pDX, IDC_MDWAVE, m_MDWave);
	DDX_Control(pDX, IDC_SWAP, m_Swap_Bttn);
	DDX_Control(pDX, IDC_REF_SYNC, m_RefSync);
	DDX_Control(pDX, IDC_COM_SYNC, m_ComSync);
	DDX_Control(pDX, IDC_TIMESP, m_TimeSpectr);
	DDX_Control(pDX, IDC_FULLRESTS, m_Fullres_Time_Spectrogram);
	DDX_Control(pDX, IDCANCEL, m_close);
	DDX_Control(pDX, IDC_USEALLDATA, m_ExtraData);
	DDX_Control(pDX, IDC_RESOLUTION, m_Resolution);
	DDX_Control(pDX, IDC_PLOT_TD, m_TimeDomain);
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
	ON_BN_CLICKED(IDC_ENABLEEXTRA, &CMDFourierGUIDlg::OnBnClickedEnableextra)
	ON_BN_CLICKED(IDC_DIFFERENCES, &CMDFourierGUIDlg::OnBnClickedDifferences)
	ON_BN_CLICKED(IDC_MISSING, &CMDFourierGUIDlg::OnBnClickedMissing)
	ON_BN_CLICKED(IDC_SPECTROGRAM, &CMDFourierGUIDlg::OnBnClickedSpectrogram)
	ON_BN_CLICKED(IDC_AVERAGE, &CMDFourierGUIDlg::OnBnClickedAverage)
	ON_BN_CLICKED(IDC_NOISEFLOOR, &CMDFourierGUIDlg::OnBnClickedNoisefloor)
	ON_BN_CLICKED(IDC_MDWAVE, &CMDFourierGUIDlg::OnBnClickedMdwave)
	ON_BN_CLICKED(IDC_SWAP, &CMDFourierGUIDlg::OnBnClickedSwap)
	ON_BN_CLICKED(IDC_TIMESP, &CMDFourierGUIDlg::OnBnClickedTimesp)
	ON_MESSAGE(WM_DROPFILES, OnDropFiles)// Message Handler for Drang and Drop
	ON_CBN_DROPDOWN(IDC_PROFILE, &CMDFourierGUIDlg::OnCbnDropdownProfile)
	ON_BN_CLICKED(IDC_PLOT_TD, &CMDFourierGUIDlg::OnBnClickedPlotTd)
END_MESSAGE_MAP()


// CMDFourierGUIDlg message handlers

BOOL CMDFourierGUIDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	m_ExtraParamsEditBox.EnableWindow(FALSE);

	if(!CheckDependencies())
	{
		EndDialog(IDCANCEL);
		return FALSE;
	}

	m_DiffBttn.SetCheck(TRUE);
	m_MissBttn.SetCheck(TRUE);
	m_SpectrBttn.SetCheck(TRUE);
	m_NoiseFloor.SetCheck(TRUE);
	m_AveragePlot_Bttn.SetCheck(TRUE);
	m_TimeSpectr.SetCheck(TRUE);
	m_ExtraData.SetCheck(TRUE);
	m_TimeDomain.SetCheck(TRUE);

	FillComboBoxes();

	m_OpenResultsBttn.EnableWindow(FALSE);
	DosWaitCount = 0;

	mdwave = false;
	killingDOS = false;

	DragAcceptFiles(TRUE);
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
	TCHAR szFilters[]= L"Audio Files (*.wav;*.flac)|*.wav;*.flac||";
	CFileDialog dlgFile(TRUE, L"wav", m_Reference, OFN_FILEMUSTEXIST, szFilters);
	
	if(dlgFile.DoModal() != IDOK)
		return;

	m_Reference = dlgFile.GetPathName();
	m_ReferenceLbl.SetWindowText(m_Reference); 
}


void CMDFourierGUIDlg::OnBnClickedSelectReferenceCompare()
{
	TCHAR szFilters[]= L"Audio Files (*.wav;*.flac)|*.wav;*.flac|MDF List (*.mfl)|*.mfl||";
	CFileDialog dlgFile(TRUE, L"wav", m_ComparisonFile, OFN_FILEMUSTEXIST, szFilters);

	if(dlgFile.DoModal() != IDOK)
		return;

	m_ComparisonFile = dlgFile.GetPathName();
	m_ComparisonLbl.SetWindowText(m_ComparisonFile);
}


void CMDFourierGUIDlg::OnBnClickedOk()
{
	CString profile;

	UpdateWindow();

	m_OpenResultsBttn.EnableWindow(FALSE);

	profile = GetSelectedCommandLineValue(m_Profiles, COUNT_PROFILES);
	if(profile == L"NONE")
	{
		MessageBox(L"Please select a profile for the comparison.", L"Action needed");
		return;
	}

	if(!cDos.m_fDone)
	{
		MessageBox(L"Please wait for results.", L"Please wait");
		return;
	}

	if(!m_Reference.GetLength())
	{
		MessageBox(L"Please select a Reference audio file.", L"Error");
		return;
	}
	if(!m_ComparisonFile.GetLength())
	{
		MessageBox(L"Please select a Comparison audio file.", L"Error");
		return;
	}
	if(m_Reference == m_ComparisonFile)
	{
		MessageBox(L"Reference and compare file are the same.\nPlease select a different file.", L"Error");
		return;
	}

	if(elements)
	{
		delete [] elements;
		elements = NULL;
	}

	elementCount = 0;
	elementPos = 0;

	if(m_ComparisonFile.Right(3).CompareNoCase(L"wav") == 0 ||
		m_ComparisonFile.Right(4).CompareNoCase(L"flac") == 0)
	{
		elements = new CString[1];
		if(!elements)
		{
			MessageBox(L"Not enough memory", L"Error");
			return;
		}
		elements[0] = m_ComparisonFile;
		elementCount = 1;
	}

	if(m_ComparisonFile.Right(3).CompareNoCase(L"mfl") == 0)
	{
		FILE *file = NULL;
		errno_t err;
		wchar_t item[2056];

		err = _wfopen_s(&file, m_ComparisonFile, L"r");
		if(err != 0)
		{
			MessageBox(L"Could not open MFL file", L"Error");
			return;
		}
		
		while(fwscanf_s(file, L"%[^\n]\n", item, 2056) == 1)
			elementCount++;

		elements = new CString[elementCount];
		if(!elements)
		{
			MessageBox(L"Not enough memory", L"Error");
			return;
		}

		fseek(file, 0, SEEK_SET);

		while(fwscanf_s(file, L"%[^\n]\n", item, 2056) == 1)
			elements[elementPos++] = item;
		
		fclose(file);
		elementPos = 0;

		listName = m_ComparisonFile;
	}

	if(elementCount)
		ExecuteCommand(elements[elementPos]);
}

void CMDFourierGUIDlg::ExecuteCommand(CString Compare)
{
	CString	command, extraCmd;
	CString	window, adjust, profile, syncFormatRef, syncFormatComp, res;

	profile = GetSelectedCommandLineValue(m_Profiles, COUNT_PROFILES);
	window = GetSelectedCommandLineValue(m_WindowTypeSelect, COUNT_WINDOWS);
	adjust = GetSelectedCommandLineValue(m_CurveAdjustSelect, COUNT_CURVES);
	res = GetSelectedCommandLineValue(m_Resolution, COUNT_RESOLUTION);
	syncFormatRef = GetSelectedCommandLineValue(m_RefSync, COUNT_SYNCTYPE);
	syncFormatComp = GetSelectedCommandLineValue(m_ComSync, COUNT_SYNCTYPE);

	command.Format(L"mdfourier.exe -P \"%s\" -r \"%s\" -c \"%s\" -w %s -o %s -Y %s -Z %s -l", 
			profile, m_Reference, Compare, window, adjust, syncFormatRef, syncFormatComp);

	if(m_AlignFFTW.GetCheck() == BST_CHECKED)
		command += " -z";

	if(m_AveragePlot_Bttn.GetCheck() == BST_CHECKED)
		command += " -g";

	if(m_VerboseLog_Bttn.GetCheck() == BST_CHECKED)
	{
		command += " -v";
		cDos.verbose = TRUE;
	}
	else
		cDos.verbose = FALSE;

	if(m_DiffBttn.GetCheck() != BST_CHECKED)
		command += " -D";
	if(m_MissBttn.GetCheck() != BST_CHECKED)
		command += " -M";
	if(m_SpectrBttn.GetCheck() != BST_CHECKED)
		command += " -S";
	if(m_NoiseFloor.GetCheck() != BST_CHECKED)
		command += " -F";
	if(m_TimeSpectr.GetCheck() != BST_CHECKED)
		command += " -t";
	else
	{
		if(m_Fullres_Time_Spectrogram.GetCheck())
			command += " -E";
	}
	if(m_TimeDomain.GetCheck() != BST_CHECKED)
		command += " -Q";

	if(m_ExtraData.GetCheck() != BST_CHECKED)
		command += " -X";

	if(m_EnableExtraBttn.GetCheck() == BST_CHECKED)
		m_ExtraParamsEditBox.GetWindowText(extraCmd);
	if(extraCmd.GetLength())
	{
		command += L" ";
		command += extraCmd;
	}

	if(m_Resolution.GetCurSel() != 1)
		command += L" -L "+res;

	mdwave = false;
	ManageWindows(FALSE);

	SetTimer(IDT_DOS, 250, 0);

	m_OutputCtrl.SetWindowText(L"");
	m_ComparisonLbl.SetWindowText(Compare);

	cDos.Start(command);
}

void CMDFourierGUIDlg::OnBnClickedCancel()
{
	CString msg;

	if(killingDOS)
	{
		msg.Format(L"%s is already being terminated, please wait", mdwave ? L"MDWave" : L"MDFourier");
		if(MessageBox(msg, L"Please wait", MB_OK))
			return;
	}

	if(cDos.m_fDone)
		CDialogEx::OnCancel();
	else
	{
		msg.Format(L"%s is currently running.\nStop it?", mdwave ? L"MDWave" : L"MDFourier");
		if(MessageBox(msg, L"Terminate?", MB_OKCANCEL) == IDOK)
		{
			DosWaitCount = 0;
			killingDOS = true;

			cDos.Lock();
			cDos.StopNow();
			cDos.m_Output = L"Process signaled for exit";
			cDos.Release();

			Sleep(1000);
		}
	}
}


void CMDFourierGUIDlg::OnTimer(UINT_PTR nIDEvent)
{
	cDos.Lock();
	m_OutputCtrl.SetWindowText(cDos.m_Output);
	cDos.Release();

	m_OutputCtrl.SendMessage(EM_SETSEL, 0, -1); //Select all. 
	m_OutputCtrl.SendMessage(EM_SETSEL, -1, -1);//Unselect and stay at the end pos
	m_OutputCtrl.SendMessage(EM_SCROLLCARET, 0, 0); //Set scrollcaret to the current Pos

    if(!cDos.m_fAbortNow && cDos.m_fDone)
	{
		int		pos = 0, errorFound = 0, finished = 0;
		CString	searchFor = L"Results stored in ";

        KillTimer(IDT_DOS);
        ManageWindows(TRUE);

		// Check is we enable the results button
		cDos.Lock();
		pos = cDos.m_Output.Find(searchFor, 0);
		cDos.Release();

		if(pos != -1)
		{
			TCHAR	pwd[MAX_PATH];

			GetCurrentDirectory(MAX_PATH, pwd);
			m_OpenResultsBttn.EnableWindow(TRUE);

			resultsFolder.Format(L"%s\\%s", pwd,
				cDos.m_Output.Right(cDos.m_Output.GetLength() - pos - searchFor.GetLength()));
			resultsFolder = resultsFolder.Left(resultsFolder.GetLength() - 2);
		}

		searchFor = "ERROR";

		cDos.Lock();
		pos = cDos.m_Output.Find(searchFor, 0);
		cDos.Release();

		if(pos != -1)
		{
			CString	errorMsg;

			cDos.Lock();
			errorMsg = cDos.m_Output.Right(cDos.m_Output.GetLength()-pos);
			cDos.Release();

			MessageBox(errorMsg, L"Error from MDFourier");
			errorFound = 1;
		}

		elementPos++;

		if(elementPos < elementCount)
		{
			if(!errorFound)
				ExecuteCommand(elements[elementPos]);
			else
				finished = 1;
		}
		else
			finished = 1;

		if(finished)
		{
			elementCount = 0;
			elementPos = 0;
			if(elements)
				delete [] elements;
			elements = NULL;

			if(listName.GetLength())
			{
				m_ComparisonLbl.SetWindowText(listName);
				listName.Empty();
			}
		}
    }

	if(cDos.m_fAbortNow)
	{
		DosWaitCount ++;
		if(DosWaitCount >= 20)
		{
			cDos.KillNow();
			cDos.m_fDone = TRUE;
			m_OutputCtrl.SetWindowText(L"Process terminated.");
			killingDOS = false;

			elementCount = 0;
			elementPos = 0;
			if(elements)
				delete [] elements;
			elements = NULL;

			KillTimer(IDT_DOS);
			ManageWindows(TRUE);

			if(listName.GetLength())
			{
				m_ComparisonLbl.SetWindowText(listName);
				listName.Empty();
			}
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

CString CMDFourierGUIDlg::GetSelectedCommandLineValue(CComboBox &Combo, int size)
{
	int selected = -1;
	CommandLineArray *DataOk;

	selected = Combo.GetCurSel();
	if(selected == CB_ERR)
		return L"-";

	DataOk = (CommandLineArray*)Combo.GetItemDataPtr(selected);
	if(!DataOk)
		return L"-";

	return DataOk->valueMDF;
}

void CMDFourierGUIDlg::InsertValueInCombo(CString Name, CString value, CommandLineArray &Data, CComboBox &Combo)
{
	int indexCB;

	Data.Name = Name;
	indexCB = Combo.AddString(Name);
	if(indexCB != CB_ERR)
		Combo.SetItemDataPtr(indexCB, &Data);
	Data.valueMDF = value;
}

void CMDFourierGUIDlg::FillComboBoxes()
{
	InsertValueInCombo(L"None", L"n", WindowConvert[0], m_WindowTypeSelect);
	InsertValueInCombo(L"Tukey", L"t", WindowConvert[1], m_WindowTypeSelect);
	InsertValueInCombo(L"Flattop", L"f", WindowConvert[2], m_WindowTypeSelect);
	InsertValueInCombo(L"Hann", L"h", WindowConvert[3], m_WindowTypeSelect);
	InsertValueInCombo(L"Hamming", L"m", WindowConvert[4], m_WindowTypeSelect);
	m_WindowTypeSelect.SetCurSel(1);

	InsertValueInCombo(L"None", L"0", CurveConvert[0], m_CurveAdjustSelect);
	InsertValueInCombo(L"Bright", L"1", CurveConvert[1], m_CurveAdjustSelect);
	InsertValueInCombo(L"High", L"2", CurveConvert[2], m_CurveAdjustSelect);
	InsertValueInCombo(L"Neutral", L"3", CurveConvert[3], m_CurveAdjustSelect);
	InsertValueInCombo(L"Low", L"4", CurveConvert[4], m_CurveAdjustSelect);
	InsertValueInCombo(L"Dimm", L"5", CurveConvert[5], m_CurveAdjustSelect);
	m_CurveAdjustSelect.SetCurSel(3);

	InsertValueInCombo(L"NTSC", L"0", SyncType[0], m_RefSync);
	InsertValueInCombo(L"PAL", L"1", SyncType[1], m_RefSync);
	m_RefSync.SetCurSel(0);

	InsertValueInCombo(L"NTSC", L"0", SyncType[0], m_ComSync);
	InsertValueInCombo(L"PAL", L"1", SyncType[1], m_ComSync);
	m_ComSync.SetCurSel(0);

	InsertValueInCombo(L"Low", L"1", Resolutions[0], m_Resolution);
	InsertValueInCombo(L"Default", L"2", Resolutions[1], m_Resolution);
	InsertValueInCombo(L"1080p", L"3", Resolutions[2], m_Resolution);
	InsertValueInCombo(L"Hi", L"4", Resolutions[3], m_Resolution);
	InsertValueInCombo(L"4K", L"5", Resolutions[4], m_Resolution);
	m_Resolution.SetCurSel(1);

}

int CMDFourierGUIDlg::FindProfiles(CString sPath, CString pattern)
{
	int count = 0;
	CFileFind finder;
	BOOL bFind = FALSE;
	CString	search;
   
	m_Profiles.ResetContent();
	search.Format(L"%s\\%s", sPath, pattern);
	bFind = finder.FindFile(search);
	while(bFind)
	{
		CString FileName;
		bFind = finder.FindNextFileW();
        
		FileName = finder.GetFileName();
		if(finder.IsDots() || finder.IsDirectory() || FileName.Right(3).CompareNoCase(pattern.Right(3)) != 0)
			continue;
		else
		{
			FILE *file;
			errno_t err;
			wchar_t text[2056];
			CString FullFileName, ProfileName;

			FullFileName.Format(L"%s\\%s", sPath, FileName);
			err = _wfopen_s(&file, FullFileName, L"r");
			if(err != 0)
			{
				CString	msg;

				msg.Format(L"Could not load Profile file: %s\n", FullFileName);
				MessageBox(msg, L"Invalid Profile File");
				return FALSE;
			}
			if(fwscanf_s(file, L"%s %*f\n", text, 2056) != 1)
			{
				CString	msg;

				msg.Format(L"Could not load Profile file: %s\n", FullFileName);
				MessageBox(msg, L"Invalid Profile File Header");
				return FALSE;
			}
			if(fwscanf_s(file, L"%255[^\n]\n", text, 2056) != 1)
			{
				CString	msg;

				msg.Format(L"Could not load Profile file: %s\n", FullFileName);
				MessageBox(msg, L"Invalid Profile Name");
				return FALSE;
			}
			ProfileName = text;
			fclose(file);
			while(ProfileName.GetLength() && ProfileName.GetAt(0) == ' ')
				ProfileName = ProfileName.Right(ProfileName.GetLength() - 1);
			if(!ProfileName.GetLength())
				ProfileName = L"Unnamed profile!";
			InsertValueInCombo(ProfileName, FullFileName, Profiles[count++], m_Profiles);
		}
	}

	finder.Close();
	InsertValueInCombo(L" Select a profile", L"NONE", Profiles[count], m_Profiles);

	m_Profiles.SetCurSel(0);
	return count;
}

int CMDFourierGUIDlg::CheckDependencies()
{
	FILE *file;
	errno_t err;
	CString	msg, pattern;
	TCHAR	pwd[MAX_PATH];

	err = _wfopen_s(&file, L"mdfourier.exe", L"r");
	if(err != 0)
	{
		GetCurrentDirectory(MAX_PATH, pwd);
		msg.Format(L"GUI ran from folder:\n%s\nPlease place mdfourier.exe in:\n %s", pwd, pwd);
		MessageBox(msg, L"Error mdfourier.exe not found");
		return FALSE;
	}
	fclose(file);

	if(!FindProfiles(L"profiles", L"*.mfn"))
	{
		GetCurrentDirectory(MAX_PATH, pwd);
		msg.Format(L"Please place profile files (*.mfn) in folder:\n %s\\profiles", pwd);
		MessageBox(msg, L"Error mdfblocks.mfn not found");
		return FALSE;
	}

	return TRUE;
}

void CMDFourierGUIDlg::ManageWindows(BOOL Enable)
{
	m_ExecuteBttn.EnableWindow(Enable);
	m_ReferenceFileBttn.EnableWindow(Enable);
	m_ComparisonFileBttn.EnableWindow(Enable);
	m_WindowTypeSelect.EnableWindow(Enable);
	m_CurveAdjustSelect.EnableWindow(Enable);
	m_AlignFFTW.EnableWindow(Enable);
	m_ExtraData.EnableWindow(Enable);
	
	m_VerboseLog_Bttn.EnableWindow(Enable);
	if(m_EnableExtraBttn.GetCheck() == BST_CHECKED)
		m_ExtraParamsEditBox.EnableWindow(Enable);
	m_EnableExtraBttn.EnableWindow(Enable);

	m_DiffBttn.EnableWindow(Enable);
	m_MissBttn.EnableWindow(Enable);
	m_SpectrBttn.EnableWindow(Enable);
	m_NoiseFloor.EnableWindow(Enable);
	m_TimeSpectr.EnableWindow(Enable);
	m_TimeDomain.EnableWindow(Enable);
	m_Fullres_Time_Spectrogram.EnableWindow(Enable);
	m_AveragePlot_Bttn.EnableWindow(Enable);

	m_Swap_Bttn.EnableWindow(Enable);
	m_MDWave.EnableWindow(Enable);

	m_RefSync.EnableWindow(Enable);
	m_ComSync.EnableWindow(Enable);

	m_Profiles.EnableWindow(Enable);
	m_Resolution.EnableWindow(Enable);

	if(mdwave)
		m_ComparisonLbl.EnableWindow(Enable);

	if(Enable)
		m_close.SetWindowText(L"Close");
	else
		m_close.SetWindowText(L"Terminate");
}


void CMDFourierGUIDlg::OnBnClickedAbout()
{
	if(MessageBox(L"MDFourier Front End\n\nArtemio Urbina 2019-2020\nCode available under GPL\n\nhttp://junkerhq.net/MDFourier/\n\nOpen website and manual?", L"About MDFourier", MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
	{
		ShellExecute(0, 0, L"http://junkerhq.net/MDFourier/", 0, 0 , SW_SHOW );
	}
}


void CMDFourierGUIDlg::OnBnClickedEnableextra()
{
	if(m_EnableExtraBttn.GetCheck() == BST_CHECKED)
		m_ExtraParamsEditBox.EnableWindow(TRUE);
	else
		m_ExtraParamsEditBox.EnableWindow(FALSE);
}

void CMDFourierGUIDlg::CheckPlotSelection(CButton &clicked)
{
	int checked = 0;

	if(m_DiffBttn.GetCheck() == BST_CHECKED)
		checked ++;
	if(m_MissBttn.GetCheck() == BST_CHECKED)
		checked ++;
	if(m_SpectrBttn.GetCheck() == BST_CHECKED)
		checked ++;
	if(m_NoiseFloor.GetCheck() == BST_CHECKED)
		checked ++;
	if(m_TimeSpectr.GetCheck() == BST_CHECKED)
		checked ++;
	if(m_TimeDomain.GetCheck() == BST_CHECKED)
		checked ++;
	if(m_AveragePlot_Bttn.GetCheck() == BST_CHECKED)
		checked ++;

	if(!checked)
		clicked.SetCheck(TRUE);
}

void CMDFourierGUIDlg::OnBnClickedDifferences()
{
	CheckPlotSelection(m_DiffBttn);
}

void CMDFourierGUIDlg::OnBnClickedMissing()
{
	CheckPlotSelection(m_MissBttn);
}

void CMDFourierGUIDlg::OnBnClickedSpectrogram()
{
	CheckPlotSelection(m_SpectrBttn);
}

void CMDFourierGUIDlg::OnBnClickedAverage()
{
	CheckPlotSelection(m_AveragePlot_Bttn);
}

void CMDFourierGUIDlg::OnBnClickedNoisefloor()
{
	CheckPlotSelection(m_NoiseFloor);
}


void CMDFourierGUIDlg::OnBnClickedTimesp()
{
	CheckPlotSelection(m_TimeSpectr);
	if(m_TimeSpectr.GetCheck() == BST_CHECKED)
		m_Fullres_Time_Spectrogram.EnableWindow(TRUE);
	else
		m_Fullres_Time_Spectrogram.EnableWindow(FALSE);
}

void CMDFourierGUIDlg::OnBnClickedPlotTd()
{
	CheckPlotSelection(m_TimeDomain);
}


void CMDFourierGUIDlg::OnBnClickedMdwave()
{
	CString profile;
	CString	command, extraCmd;
	CString	window, syncFormat;

	UpdateWindow();

	m_OpenResultsBttn.EnableWindow(FALSE);

	profile = GetSelectedCommandLineValue(m_Profiles, COUNT_PROFILES);
	if(profile == L"NONE")
	{
		MessageBox(L"Please select a profile for the segmentation process.", L"Action needed");
		return;
	}

	if(!cDos.m_fDone)
	{
		MessageBox(L"Please wait for results.", L"Please wait");
		return;
	}

	if(!m_Reference.GetLength())
	{
		MessageBox(L"Please select a Reference audio file.", L"Error");
		return;
	}

	profile = GetSelectedCommandLineValue(m_Profiles, COUNT_PROFILES);
	window = GetSelectedCommandLineValue(m_WindowTypeSelect, COUNT_WINDOWS);
	syncFormat = GetSelectedCommandLineValue(m_RefSync, COUNT_SYNCTYPE);

	command.Format(L"mdwave.exe -P \"%s\" -r \"%s\" -w %s -Y %s -l -c", 
			profile, m_Reference, window, syncFormat);

	if(m_AlignFFTW.GetCheck() == BST_CHECKED)
		command += " -z";

	if(m_VerboseLog_Bttn.GetCheck() == BST_CHECKED)
	{
		command += " -v";
		cDos.verbose = TRUE;
	}
	else
		cDos.verbose = FALSE;

	if(m_EnableExtraBttn.GetCheck() == BST_CHECKED)
		m_ExtraParamsEditBox.GetWindowText(extraCmd);
	if(extraCmd.GetLength())
	{
		command += L" ";
		command += extraCmd;
	}

	mdwave = true;
	ManageWindows(FALSE);

	SetTimer(IDT_DOS, 250, 0);

	m_OutputCtrl.SetWindowText(L"");
	//m_ComparisonLbl.SetWindowText(L"");
	//m_ComparisonFile = L"";

	cDos.Start(command);
}

void CMDFourierGUIDlg::OnBnClickedSwap()
{
	int pos = 0, sel = 0;
	CString tmp;

	if(!m_Reference.GetLength() && !m_ComparisonFile.GetLength())
		return;
	tmp = m_ComparisonFile;
	tmp.MakeLower();
	pos = tmp.Find(L".mfl", 0);
	if(pos != -1)
		return;
	
	tmp = m_Reference;

	m_Reference = m_ComparisonFile;
	m_ReferenceLbl.SetWindowText(m_Reference); 
	m_ComparisonFile = tmp;
	m_ComparisonLbl.SetWindowText(m_ComparisonFile);

	sel = m_RefSync.GetCurSel();
	m_RefSync.SetCurSel(m_ComSync.GetCurSel());
	m_ComSync.SetCurSel(sel);
}

bool CMDFourierGUIDlg::VerifyFileExtension(CString filename, int type)
{
	if(filename.Right(3).CompareNoCase(L"wav") == 0)
		return true;
	if(filename.Right(4).CompareNoCase(L"flac") == 0)
		return true;
	if(type == 1 && filename.Right(3).CompareNoCase(L"mfl") == 0)
		return true;
	return false;
}

LRESULT CMDFourierGUIDlg::OnDropFiles(WPARAM wParam, LPARAM lParam)
{
	TCHAR	szDroppedFile[MAX_PATH];
	HDROP	hDrop ;
	int		nFiles;
	bool	erase = false;

	hDrop = (HDROP)wParam;
	UpdateData();
	
	if(!cDos.m_fDone)
		return 0;

	nFiles = DragQueryFile(hDrop, -1, szDroppedFile, MAX_PATH);
	if(nFiles > 1)
	{
		int valid = 0;

		for(int i = 0; i < nFiles; i++)
		{
			DragQueryFile(hDrop, i, szDroppedFile, MAX_PATH);
			if(VerifyFileExtension(szDroppedFile, i))
				valid ++;
		}

		if(valid == nFiles && nFiles == 2)
		{
			for(int i = 0; i < nFiles; i++)
			{
				DragQueryFile(hDrop, i, szDroppedFile, MAX_PATH);
				if(VerifyFileExtension(szDroppedFile, i))
				{
					if(i == 0)
					{
						m_Reference = szDroppedFile;
						m_ReferenceLbl.SetWindowText(m_Reference); 
						erase = true;
					}
					if(i == 1)
					{
						m_ComparisonFile = szDroppedFile;
						m_ComparisonLbl.SetWindowText(m_ComparisonFile);
						erase = true;
					}
				}
			}
		}
	}
	else
	{
		DragQueryFile(hDrop, 0, szDroppedFile, MAX_PATH);
		if(VerifyFileExtension(szDroppedFile, m_Reference.GetLength() && !m_ComparisonFile.GetLength()))
		{
			if(m_Reference.GetLength() && m_ComparisonFile.GetLength())
			{
				m_Reference.Empty();
				m_ReferenceLbl.SetWindowText(m_Reference); 
				m_ComparisonFile.Empty();
				m_ComparisonLbl.SetWindowText(m_ComparisonFile);
				erase = true;
			}

			if(!m_Reference.GetLength())
			{
				m_Reference = szDroppedFile;
				m_ReferenceLbl.SetWindowText(m_Reference); 
				erase = true;
			}
			else
			{
				m_ComparisonFile = szDroppedFile;
				m_ComparisonLbl.SetWindowText(m_ComparisonFile);
				erase = true;
			}
		}
	}
	if(erase)
		m_OutputCtrl.SetWindowText(L"");
	return 0;

}

void CMDFourierGUIDlg::OnCbnDropdownProfile()
{
	if(!FindProfiles(L"profiles", L"*.mfn"))
	{
		CString msg;
		TCHAR	pwd[MAX_PATH];

		GetCurrentDirectory(MAX_PATH, pwd);
		msg.Format(L"Please place profile files (*.mfn) in folder:\n %s\\profiles", pwd);
		MessageBox(msg, L"Error mdfblocks.mfn not found");
	}
}
