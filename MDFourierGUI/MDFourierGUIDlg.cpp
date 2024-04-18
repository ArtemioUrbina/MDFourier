
// MDFourierGUIDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MDFourierGUI.h"
#include "MDFourierGUIDlg.h"
#include "afxdialogex.h"
#include "Warnings.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMDFourierGUIDlg dialog
CMDFourierGUIDlg::CMDFourierGUIDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CMDFourierGUIDlg::IDD, pParent)
	, m_ComparisonFile(_T(""))
	, m_ReferenceFile(_T(""))
	, m_OutputText(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	elementCount = 0;
	elements = NULL;
	elementPos = 0;
	Monitor_vert = Monitor_horz = -1;
}

void CMDFourierGUIDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_REFERENCE_FILE, m_ReferenceFileLbl);
	DDX_Control(pDX, IDC_COMPARISON_FILE, m_ComparisonLbl);
	DDX_Control(pDX, IDC_OUTPUT, m_OutputTextCtrl);
	DDX_Control(pDX, ID_OPENRESULTS, m_OpenResultsBttn);
	DDX_Control(pDX, IDOK, m_ExecuteBttn);
	DDX_Control(pDX, IDC_WINDOW, m_WindowTypeSelect);
	DDX_Control(pDX, IDC_CURVEADJUST, m_CurveAdjustSelect);
	DDX_Control(pDX, IDC_SELECT_REFERENCE_FILE, m_ReferenceFileFileBttn);
	DDX_Control(pDX, IDC_SELECT_REFERENCE_COMPARE, m_ComparisonFileBttn);
	DDX_Control(pDX, IDC_ALIGN, m_AlignFFTWCheckBox);
	DDX_Control(pDX, IDC_AVERAGE, m_AveragePlotCheckBox);
	DDX_Control(pDX, IDC_VERBOSE, m_VerboseLogCheckBox);
	DDX_Control(pDX, IDC_EXTRACLPARAMS, m_ExtraParamsEditBox);
	DDX_Control(pDX, IDC_ENABLEEXTRA, m_EnableExtraCommandCheckBox);
	DDX_Control(pDX, IDC_DIFFERENCES, m_DifferencesCheckBox);
	DDX_Control(pDX, IDC_MISSING, m_MissingExtraCheckBox);
	DDX_Control(pDX, IDC_SPECTROGRAM, m_SpectrogramsCheckBox);
	DDX_Control(pDX, IDC_PROFILE, m_Profiles);
	DDX_Control(pDX, IDC_NOISEFLOOR, m_NoiseFloorCheckBox);
	DDX_Control(pDX, IDC_MDWAVE, m_MDWaveBttn);
	DDX_Control(pDX, IDC_SWAP, m_SwapBttn);
	DDX_Control(pDX, IDC_REF_SYNC, m_RefSync);
	DDX_Control(pDX, IDC_COM_SYNC, m_ComSync);
	DDX_Control(pDX, IDC_TIMESP, m_TimeSpectrogramCheckBox);
	DDX_Control(pDX, IDC_FULLRESTS, m_FullResTimeSpectrCheckBox);
	DDX_Control(pDX, IDCANCEL, m_CloseBttn);
	DDX_Control(pDX, IDC_USEALLDATA, m_ExtraDataCheckBox);
	DDX_Control(pDX, IDC_RESOLUTION, m_Resolution);
	DDX_Control(pDX, IDC_PLOT_TD, m_WaveFormCheckBox);
	DDX_Control(pDX, IDC_PHASE, m_PhaseCheckBox);
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
	ON_BN_CLICKED(IDC_PHASE, &CMDFourierGUIDlg::OnBnClickedPhase)
	ON_CBN_SELENDOK(IDC_PROFILE, &CMDFourierGUIDlg::OnCbnSelendokProfile)
	ON_CBN_SELENDCANCEL(IDC_PROFILE, &CMDFourierGUIDlg::OnCbnSelendcancelProfile)
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

	m_DifferencesCheckBox.SetCheck(TRUE);
	m_MissingExtraCheckBox.SetCheck(TRUE);
	m_SpectrogramsCheckBox.SetCheck(TRUE);
	m_NoiseFloorCheckBox.SetCheck(TRUE);
	m_AveragePlotCheckBox.SetCheck(TRUE);
	m_TimeSpectrogramCheckBox.SetCheck(TRUE);
	m_ExtraDataCheckBox.SetCheck(TRUE);
	m_WaveFormCheckBox.SetCheck(TRUE);
	m_PhaseCheckBox.SetCheck(TRUE);

	ReduceWindowSizeIfLowRes();
	FillComboBoxes();

	m_OpenResultsBttn.EnableWindow(FALSE);
	DosWaitCount = 0;

	mdwave = false;
	killingDOS = false;

	DragAcceptFiles(TRUE);
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CMDFourierGUIDlg::ReduceWindowSizeIfLowRes()
{
	HMONITOR hmon;
    MONITORINFO mi = {0};

    mi.cbSize = sizeof(mi);
	hmon = ::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY);
    if(::GetMonitorInfo(hmon, &mi))
    {
        //Got monitor size & position where the process was started in
        CRect rcThis;
        this->GetWindowRect(rcThis);

		int sizex = mi.rcWork.right - mi.rcWork.left;
		int sizey = mi.rcWork.bottom - mi.rcWork.top;

		int windowH = rcThis.Height();
		// We take working area, not full resolution
		if(sizey > 1900)
		{
			Monitor_vert = mi.rcMonitor.bottom - mi.rcMonitor.top;
			Monitor_horz = mi.rcMonitor.right - mi.rcMonitor.left;
		}
		else
		{
			Monitor_vert = sizey; 
			Monitor_horz = sizex; 
		}
		if(mi.rcMonitor.bottom - mi.rcMonitor.top < 600 || Monitor_horz < 800)
		{
			MessageBox(L"Resolution needs to be at least 800x600, sorry", L"MDFourier");
			EndDialog(IDCANCEL);
			return;
		}
		if(windowH > sizey)
		{
			CRect rect;
			int targetH = sizey*.8;
			int ctrlH = 0;
			int margin = 50;
			
			//Move buttons
			m_ExecuteBttn.GetWindowRect(&rect);
			ctrlH = rect.Height();
			rect.top = targetH - ctrlH - margin;
			rect.bottom = targetH - margin;
			m_ExecuteBttn.MoveWindow(&rect);

			m_CloseBttn.GetWindowRect(&rect);
			ctrlH = rect.Height();
			rect.top = targetH - ctrlH - margin;
			rect.bottom = targetH - margin;
			m_CloseBttn.MoveWindow(&rect);

			m_OpenResultsBttn.GetWindowRect(&rect);
			ctrlH = rect.Height();
			rect.top = targetH - ctrlH - margin;
			rect.bottom = targetH - margin;
			m_OpenResultsBttn.MoveWindow(&rect);

			// Resize Output
			int buttonsTop = rect.top;
			m_OutputTextCtrl.GetWindowRect(&rect);
			rect.top -= ctrlH;
			rect.bottom = buttonsTop - ctrlH / 2;
			m_OutputTextCtrl.MoveWindow(&rect);

			this->SetWindowPos(NULL, 0, 0, rcThis.Width(), targetH, SWP_NOMOVE | SWP_NOZORDER);
			this->GetWindowRect(rcThis);
			windowH = rcThis.Height();
		}
        int x = (sizex - rcThis.Width()) / 2;
        int y = (sizey - rcThis.Height()) / 2;

        this->MoveWindow(mi.rcWork.left + x, mi.rcWork.top + y, rcThis.Width(), rcThis.Height());
    }
    else
        this->CenterWindow();
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
	CFileDialog dlgFile(TRUE, L"wav", m_ReferenceFile, OFN_FILEMUSTEXIST, szFilters);
	
	if(dlgFile.DoModal() != IDOK)
		return;

	m_ReferenceFile = dlgFile.GetPathName();
	m_ReferenceFileLbl.SetWindowText(m_ReferenceFile); 
	m_OpenResultsBttn.EnableWindow(FALSE);
}


void CMDFourierGUIDlg::OnBnClickedSelectReferenceCompare()
{
	TCHAR szFilters[]= L"Audio Files (*.wav;*.flac)|*.wav;*.flac|MDF List (*.mfl)|*.mfl||";
	CFileDialog dlgFile(TRUE, L"wav", m_ComparisonFile, OFN_FILEMUSTEXIST, szFilters);

	if(dlgFile.DoModal() != IDOK)
		return;

	m_ComparisonFile = dlgFile.GetPathName();
	m_ComparisonLbl.SetWindowText(m_ComparisonFile);
	m_OpenResultsBttn.EnableWindow(FALSE);
}


void CMDFourierGUIDlg::OnBnClickedOk()
{
	CString profile;

	UpdateWindow();

	ClearResults();

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

	if(!m_ReferenceFile.GetLength())
	{
		MessageBox(L"Please select a Reference audio file.", L"Error");
		return;
	}
	if(!m_ComparisonFile.GetLength())
	{
		MessageBox(L"Please select a Comparison audio file.", L"Error");
		return;
	}
	if(m_ReferenceFile == m_ComparisonFile)
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
		wchar_t item[BUFFER_SIZE];

		err = _wfopen_s(&file, m_ComparisonFile, L"r");
		if(err != 0)
		{
			MessageBox(L"Could not open MFL file", L"Error");
			return;
		}
		
		while(fwscanf_s(file, L"%[^\n]\n", item, BUFFER_SIZE) == 1)
			elementCount++;

		elements = new CString[elementCount];
		if(!elements)
		{
			MessageBox(L"Not enough memory", L"Error");
			return;
		}

		fseek(file, 0, SEEK_SET);

		while(fwscanf_s(file, L"%[^\n]\n", item, BUFFER_SIZE) == 1)
			elements[elementPos++] = item;
		
		fclose(file);
		elementPos = 0;

		listName = m_ComparisonFile;
	}

	if(elementCount)
	{
		multiErrors.Empty();
		multiWarnings.Empty();
		ExecuteCommand(elements[elementPos]);
		if(elementCount > 1)
			ChangeWindowText(L"0%");
	}
}

void CMDFourierGUIDlg::ExecuteCommand(CString Compare)
{
	CString	command, extraCmd;
	CString	window, adjust, profile, syncFormatRef, syncFormatComp, res;

	profile = GetSelectedCommandLineValue(m_Profiles, COUNT_PROFILES);
	window = GetSelectedCommandLineValue(m_WindowTypeSelect, COUNT_WINDOWS);
	adjust = GetSelectedCommandLineValue(m_CurveAdjustSelect, COUNT_CURVES);

	command.Format(L"mdfourier.exe -P \"%s\" -r \"%s\" -c \"%s\" -w %s -o %s", 
			profile, m_ReferenceFile, Compare, window, adjust);

	if(syncTypes)
	{
		CString syncData;

		syncFormatRef = GetSelectedCommandLineValue(m_RefSync, syncTypes);
		syncFormatComp = GetSelectedCommandLineValue(m_ComSync, syncTypes);
		syncData.Format(L" -Y %s -Z %s", syncFormatRef, syncFormatComp);
		command += syncData;
	}

	if(m_AlignFFTWCheckBox.GetCheck() == BST_CHECKED && extraCmd.Find(L"-z") == -1)
		command += " -z";

	if(m_AveragePlotCheckBox.GetCheck() != BST_CHECKED && extraCmd.Find(L"-g") == -1)
		command += " -g";

	if(m_VerboseLogCheckBox.GetCheck() == BST_CHECKED && extraCmd.Find(L"-v") == -1)
	{
		command += " -v";
		cDos.verbose = TRUE;
	}
	else
		cDos.verbose = FALSE;

	if(m_DifferencesCheckBox.GetCheck() != BST_CHECKED && extraCmd.Find(L"-D") == -1)
		command += " -D";
	if(m_MissingExtraCheckBox.GetCheck() != BST_CHECKED && extraCmd.Find(L"-M") == -1)
		command += " -M";
	if(m_SpectrogramsCheckBox.GetCheck() != BST_CHECKED && extraCmd.Find(L"-S") == -1)
		command += " -S";
	if(m_NoiseFloorCheckBox.GetCheck() != BST_CHECKED && extraCmd.Find(L"-F") == -1)
		command += " -F";
	if(m_TimeSpectrogramCheckBox.GetCheck() != BST_CHECKED && extraCmd.Find(L"-t") == -1)
		command += " -t";
	else
	{
		if(m_FullResTimeSpectrCheckBox.GetCheck() && extraCmd.Find(L"-E") == -1)
			command += " -E";
	}
	if(m_WaveFormCheckBox.GetCheck() != BST_CHECKED && extraCmd.Find(L"-Q") == -1)
		command += " -Q";
	if(m_PhaseCheckBox.GetCheck() != BST_CHECKED && extraCmd.Find(L"-O") == -1)
		command += " -O";

	if(m_ExtraDataCheckBox.GetCheck() != BST_CHECKED && extraCmd.Find(L"-X") == -1)
		command += " -X";

	if(m_EnableExtraCommandCheckBox.GetCheck() == BST_CHECKED)
		m_ExtraParamsEditBox.GetWindowText(extraCmd);
	if(extraCmd.GetLength())
	{
		command += L" ";
		command += extraCmd;
	}

	if(extraCmd.Find(L"-L") == -1)
	{
		res = GetSelectedCommandLineValue(m_Resolution, COUNT_RESOLUTION);
		command += L" -L "+res;
	}

	mdwave = false;
	killingDOS = false;
	ManageWindows(FALSE);

	SetTimer(IDT_DOS, 100, 0);

	m_OutputTextCtrl.SetWindowText(L"");
	m_ComparisonLbl.SetWindowText(Compare);

	cDos.Start(command);
}

void CMDFourierGUIDlg::OnBnClickedCancel()
{
	CString msg;

	if(cDos.m_fDone)
	{
		if(MessageBox(L"Really close MDFourier?", L"Close MDFourier", MB_OKCANCEL)==IDOK)
			CDialogEx::OnCancel();
	}
	else
	{
		if(killingDOS)
		{
			msg.Format(L"%s is already being terminated, please wait", mdwave ? L"MDWave" : L"MDFourier");
			if(MessageBox(msg, L"Please wait", MB_OK))
				return;
		}

		msg.Format(L"%s is currently running.\nStop it?", mdwave ? L"MDWave" : L"MDFourier");
		if(MessageBox(msg, L"Terminate?", MB_OKCANCEL) == IDOK)
		{
			DosWaitCount = 0;
			killingDOS = true;

			cDos.Lock();
			cDos.StopNow();
			cDos.m_OutputText = L"Process signaled for exit";
			cDos.Release();

			Sleep(1000);
		}
	}
}

void CMDFourierGUIDlg::ReadAndDisplayResults(CString &newText)
{
	CString		ntext;
	int			newLineCount;

	cDos.Lock();
	ntext = cDos.m_OutputText;
	cDos.Release();

	if(ntext != cmdWindowText)
	{
		m_OutputTextCtrl.SetWindowText(ntext);
		newLineCount = ntext.Replace(_T("\n"), _T("\n"));		
		m_OutputTextCtrl.SendMessage(EM_LINESCROLL, 0, newLineCount);
		cmdWindowText = ntext;
	}
	newText = ntext;
}

void CMDFourierGUIDlg::CheckForDifferenceplots(CString ntext)
{
	int pos, end, length;
	CString searchFor = L" - Preliminary results in ";

	if (m_OpenResultsBttn.IsWindowEnabled())
		return;

	// Check is we enable the results button
	pos = ntext.Find(searchFor, 0);
	if (pos != -1)
	{
		CString extraCmd;

		pos += searchFor.GetLength();
		end = ntext.Find(L"\n", pos);
		length = end - pos;
		if (m_EnableExtraCommandCheckBox.GetCheck() == BST_CHECKED)
			m_ExtraParamsEditBox.GetWindowText(extraCmd);
		if (extraCmd.Find(L"-0") == -1)
		{
			TCHAR	pwd[MAX_PATH];

			GetCurrentDirectory(MAX_PATH, pwd);
			m_ResultsFolderText.Format(L"%s\\%s", pwd, ntext.Mid(pos, length));
			
		}
		else
			m_ResultsFolderText.Format(L"%s", ntext.Mid(pos, length));
		m_ResultsFolderText = m_ResultsFolderText.Left(m_ResultsFolderText.GetLength() - 1);

		m_OpenResultsBttn.EnableWindow(TRUE);
	}
}

void CMDFourierGUIDlg::OnTimer(UINT_PTR nIDEvent)
{
	CString		ntext;
	
	ReadAndDisplayResults(ntext);

	CheckForDifferenceplots(ntext);
	if(cDos.m_fAbortNow)
	{
		if(DosWaitCount >= 20 || cDos.m_fDone)
		{
			if(!cDos.m_fDone)
			{
				cDos.KillNow();
				cDos.m_fDone = TRUE;
				m_OutputTextCtrl.SetWindowText(L"Process killed.");
			}
			else
				m_OutputTextCtrl.SetWindowText(L"Process terminated.");
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
			ChangeWindowText();
		}
		DosWaitCount ++;
	}
	else
	{
		int			pos = 0, finished = 0, warncount = 0;;
		CString		searchFor = L"Results stored in ";

		if(cDos.m_fDone)
		{
			// In case cDos.m_fDone flag was raised between updates, has happened a lot
			ReadAndDisplayResults(ntext);

			KillTimer(IDT_DOS);
			ManageWindows(TRUE);

			// Check is we enable the results button
			pos = ntext.Find(searchFor, 0);
			if(pos != -1)
			{
				CString extraCmd;

				m_OpenResultsBttn.EnableWindow(TRUE);
				if(m_EnableExtraCommandCheckBox.GetCheck() == BST_CHECKED)
					m_ExtraParamsEditBox.GetWindowText(extraCmd);
				if(extraCmd.Find(L"-0") == -1)
				{
					TCHAR	pwd[MAX_PATH];

					GetCurrentDirectory(MAX_PATH, pwd);
					m_ResultsFolderText.Format(L"%s\\%s", pwd,
						ntext.Right(ntext.GetLength() - pos - searchFor.GetLength()));
				}
				else
					m_ResultsFolderText.Format(L"%s", ntext.Right(ntext.GetLength() - pos - searchFor.GetLength()));
				m_ResultsFolderText = m_ResultsFolderText.Left(m_ResultsFolderText.GetLength() - 2);
			}

			searchFor = L"ERROR";
			pos = ntext.Find(searchFor, 0);
			if(pos != -1)
			{
				int	end = 0;
				CString	errorMsg;

				errorMsg = ntext.Right(ntext.GetLength()-pos);
				end = errorMsg.Find(L"Aborting", 0);
				if(end != -1)
					errorMsg = errorMsg.Left(end);
				if(elementCount > 1)
				{
					multiErrors += L"File: ";
					multiErrors += elements[elementPos];
					multiErrors += "\r\n";
				}

				multiErrors += errorMsg;
				multiErrors += "\r\n";
			}

			searchFor = L"WARNING";
			pos = 0;
			do {
				int insert = 0;

				pos = ntext.Find(searchFor, pos+1);
				if(pos != -1)
				{
					int	end = 0;
					CString	warnMsg;

					warnMsg = ntext.Right(ntext.GetLength()-pos);
					end = warnMsg.Find(L"\n", 0);
					if(end != -1)
						warnMsg = warnMsg.Left(end);
					if(elementCount > 1 && !warncount)
					{
						multiWarnings += L"File: ";
						multiWarnings += elements[elementPos];
						multiWarnings += "\r\n";
					}
					if(warncount > 0)
					{
						int check = 0;

						check = multiWarnings.Find(warnMsg, 0);
						if(check == -1)
							insert = 1;
					}
					else
						insert = 1;
					if(insert)
					{
						multiWarnings += warnMsg;
						multiWarnings += "\r\n";
						warncount ++;
					}
				}
			}while(pos != -1);

			if(elementCount > 1 && warncount)
				multiWarnings += L"\r\n";

			elementPos++;

			if(elementPos < elementCount)
			{

				double percent = 0;
				CString msg;

				percent = (double)elementPos/(double)elementCount*100.0;
				msg.Format(L"%0.1f%%", percent);
				ExecuteCommand(elements[elementPos]);
				ChangeWindowText(msg);
			}
			else
				finished = 1;

			if(finished)
			{
				if(listName.GetLength())
				{
					m_ComparisonLbl.SetWindowText(listName);
					listName.Empty();
				}

				if(multiErrors.GetLength())
				{
					if(elementCount > 1)
					{
						CWarnings Warnings;

						Warnings.SetWarnings(multiErrors, true);
						Warnings.DoModal();
					}
					else
						MessageBox(multiErrors, L"Error from MDFourier");
				}

				if(multiWarnings.GetLength())
				{
					if(elementCount > 1)
					{
						CWarnings Warnings;

						Warnings.SetWarnings(multiWarnings);
						Warnings.DoModal();
					}
					else
					{
						if(!multiErrors.GetLength())
							MessageBox(multiWarnings, L"Warning from MDFourier");
					}
				}

				multiErrors.Empty();
				multiWarnings.Empty();
				elementCount = 0;
				elementPos = 0;
				if(elements)
					delete [] elements;
				elements = NULL;

				ChangeWindowText();
			}
		}
    }
	CDialogEx::OnTimer(nIDEvent);
}


void CMDFourierGUIDlg::OnBnClickedOpenresults()
{
	PIDLIST_ABSOLUTE pidl;

	if(!m_ResultsFolderText.GetLength())
		return;

	if (SUCCEEDED(SHParseDisplayName(CT2W(m_ResultsFolderText), 0, &pidl, 0, 0)))
	{
		ITEMIDLIST idNull = { 0 };
		LPCITEMIDLIST pidlNull[1] = { &idNull };
		SHOpenFolderAndSelectItems(pidl, 1, pidlNull, 0);
		ILFree(pidl);
	}
	else
	{
		CString	msg;

		msg.Format(L"Could not open folder:\n%s", m_ResultsFolderText);
		MessageBox(msg, L"Invalid Folder");
		ClearResults();
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
	InsertValueInCombo(L"Dim", L"5", CurveConvert[5], m_CurveAdjustSelect);
	m_CurveAdjustSelect.SetCurSel(3);

	InsertValueInCombo(L"500p", L"0", Resolutions[0], m_Resolution);
	InsertValueInCombo(L"600p", L"1", Resolutions[1], m_Resolution);
	InsertValueInCombo(L"900p", L"2", Resolutions[2], m_Resolution);
	InsertValueInCombo(L"1080p", L"3", Resolutions[3], m_Resolution);
	InsertValueInCombo(L"1800p", L"4", Resolutions[4], m_Resolution);
	InsertValueInCombo(L"2160p", L"5", Resolutions[5], m_Resolution);
	InsertValueInCombo(L"4320p", L"6", Resolutions[6], m_Resolution);
	InsertValueInCombo(L"8640p", L"7", Resolutions[7], m_Resolution);
	//InsertValueInCombo(L"32K", L"8", Resolutions[7], m_Resolution);
	//InsertValueInCombo(L"64K", L"9", Resolutions[8], m_Resolution);
	m_Resolution.SetCurSel(2);
	// Auto select 1080p or 4K, etc
	if(Monitor_vert != -1)
	{
		if(Monitor_vert > 4320)
			m_Resolution.SetCurSel(6);
		else if(Monitor_vert > 2160)
			m_Resolution.SetCurSel(5);
		else if(Monitor_vert > 1800)
			m_Resolution.SetCurSel(4);
		else if(Monitor_vert > 1080)
			m_Resolution.SetCurSel(3);
		else if(Monitor_vert > 900)
			m_Resolution.SetCurSel(2);
		else if(Monitor_vert > 600)
			m_Resolution.SetCurSel(1);
		else
			m_Resolution.SetCurSel(0);
	}
}

int CMDFourierGUIDlg::LoadProfile(CString FullFileName, CString &Name, CString &Version, CString &Error, CommandLineArray ProfileSyncTypes[], int ArraySize, int &syncCount)
{
	FILE *file;
	errno_t err;
	wchar_t text[BUFFER_SIZE], version[BUFFER_SIZE];
	CString ProfileName;
	int		count = 0, i = 0, regularProfile = 0;

	syncCount = 0;
	err = _wfopen_s(&file, FullFileName, L"r");
	if(err != 0)
	{
		CString	msg;

		msg.Format(L"Could not open Profile file: %s\n", FullFileName);
		Error = msg;
		return 0;
	}
	/* Header */
	if(fwscanf_s(file, L"%s %s\n", text, BUFFER_SIZE, version, BUFFER_SIZE) != 2)
	{
		CString	msg;

		msg.Format(L"Could not load Profile file, invalid version: %s\n", FullFileName);
		Error = msg;
		return 0;
	}

	if(wcscmp(text, L"MDFourierAudioBlockFile") == 0)
		regularProfile = 1;

	Version = version;
	/* Only process matching versions */
	if(_wtof(version) != _wtof(ProfileVersion))
	{
		fclose(file);
		return -1;
	}
	
	/* Name */
	if(fwscanf_s(file, L"%255[^\n]\n", text, BUFFER_SIZE) != 1)
	{
		CString	msg;

		msg.Format(L"Could not load Profile file, invalid name line: %s\n", FullFileName);
		Error = msg;
		return 0;
	}
	ProfileName = text;
				
	while(ProfileName.GetLength() && ProfileName.GetAt(0) == ' ')
		ProfileName = ProfileName.Right(ProfileName.GetLength() - 1);
	if(!ProfileName.GetLength())
		ProfileName = L"Unnamed profile!";
	Name = ProfileName;

	if(!regularProfile)
	{
		fclose(file);
		return 1;
	}

	if(fwscanf_s(file, L"[VideoModes] %s\n", text, BUFFER_SIZE) != 1)
	{
		CString	msg;

		msg.Format(L"Could not load Profile file, invalid sync count: %s\n", text);
		Error = msg;
		fclose(file);
		return 0;
	}
	count = _wtoi(text);
	if(!count)
	{
		CString	msg;

		msg.Format(L"Could not load Profile file, invalid sync count: %s\n", text);
		Error = msg;
		fclose(file);
		return 0;
	}
	if(ArraySize < count)
	{
		CString	msg;

		msg.Format(L"Could not load Profile file, sync count too big: %s\n", text);
		Error = msg;
		fclose(file);
		return 0;
	}

	for(i = 0; i < count; i++)
	{
		if(fwscanf_s(file, L"%s %*f %*d %*d %*d %*d\n", text, BUFFER_SIZE) == 1)
		{
			ProfileSyncTypes[i].Name = text;
			ProfileSyncTypes[i].valueMDF.Format(L"%d", i);
		}
		else
		{
			CString	msg;

			msg.Format(L"Could not load Profile file, invalid sync lineg: %s\n", text);
			Error = msg;
			fclose(file);
			return 0;
		}
	}
	fclose(file);
	syncCount = count;
	return 1;
}

int CMDFourierGUIDlg::FindProfiles(CString sPath, CString pattern)
{
	int count = 0, nonmatching = 0;
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
			int					match = 0, syncCount;
			CString				Name, Version, Error, FullFileName;
			CommandLineArray	ProfileSyncTypes[COUNT_SYNCTYPE];

			FullFileName.Format(L"%s\\%s", sPath, FileName);
			match = LoadProfile(FullFileName, Name, Version, Error, ProfileSyncTypes, COUNT_SYNCTYPE, syncCount);
			if(match == 0)
			{
				MessageBox(Error, L"Invalid Profile File");
				return FALSE;
			}
			if(match < 0)
				nonmatching ++;
			else
				InsertValueInCombo(Name, FullFileName, Profiles[count++], m_Profiles);
		}
	}

	finder.Close();
	InsertValueInCombo(L" Select a profile", L"NONE", Profiles[count], m_Profiles);

	m_Profiles.SetCurSel(0);
	if(!count && nonmatching)
		return -1*nonmatching;
	
	return count;
}

void CMDFourierGUIDlg::ChangeWindowText(CString data)
{
	CString newName;

	if(data.GetLength())
	{
		newName.Format(L"%s -- %s -- %s", baseWintext, data, wintextProfile);
		SetWindowText(newName);
	}
	else
	{
		if(wintextProfile.GetLength())
		{
			newName.Format(L"%s -- %s", baseWintext, wintextProfile);
			SetWindowText(newName);
		}
		else
			SetWindowText(baseWintext);
	}
}

int CMDFourierGUIDlg::CheckDependencies()
{
	FILE *file;
	errno_t err;
	CString	msg, pattern, bits, versionText, version, readText, profile;
	TCHAR	pwd[MAX_PATH];
	int counter = 0, error = 0, matched = 0;
	BOOL finished = 0;

	/* Check MDF version and test binary*/

	err = _wfopen_s(&file, L"mdfourier.exe", L"r");
	if(err != 0)
	{
		GetCurrentDirectory(MAX_PATH, pwd);
		msg.Format(L"GUI ran from folder:\n%s\nPlease place mdfourier.exe in:\n %s", pwd, pwd);
		MessageBox(msg, L"Error mdfourier.exe not found");
		return FALSE;
	}
	fclose(file);

	cDos.Start(L"mdfourier.exe -V");
	do
	{
		finished = cDos.m_fDone;
		if(!finished)
		{
			counter ++;
			if(counter < 20)
				Sleep(100);
			else
			{
				cDos.Lock();
				cDos.KillNow();
				cDos.Release();

				msg.Format(L"MDFourier command could not be executed:\n%s", cDos.m_OutputText);
				MessageBox(msg, L"Error mdfourier.exe not working");
				return FALSE;
			}
		}
	}while(!finished);

	cDos.Lock();
	readText = cDos.m_OutputText;
	cDos.Release();

	versionText = L"version ";
	if(readText.Find(versionText) == 0)
	{
		int pos = 0;

		pos = readText.Right(readText.GetLength()-versionText.GetLength()).Find(' ');
		if(pos != -1)
		{
			version = readText.Right(readText.GetLength()-versionText.GetLength()).Left(pos);
			readText = readText.Right(readText.GetLength()-versionText.GetLength()).Mid(pos+1);
			pos = readText.Find(' ');
			if(pos != -1)
			{
				bits = readText.Left(pos);
				profile = readText.Mid(pos+1);;
			}
			else
				error = 4;
			if(version != MDFVERSION)
				error = 3;
		}
		else
			error = 2;
	}
	else
		error = 1;
	if(error)
	{
		msg.Format(L"Invalid mdfourier.exe version.\nExpected:\n %s\nGot:\n %s %s", 
			MDFVERSION, version, readText);
		MessageBox(msg, L"Error improper mdfourier.exe");
		return FALSE;
	}
	MDFVersion = readText;
	ProfileVersion = profile;
	baseWintext.Format(L"MDFourier [%s/%s]", version, bits);
	ChangeWindowText();

	/* Check profiles */
	matched = FindProfiles(L"profiles", L"*.mfn");
	if(matched <= 0)
	{
		CString msg;
		TCHAR	pwd[MAX_PATH];

		GetCurrentDirectory(MAX_PATH, pwd);

		if(matched == 0)
		{
			msg.Format(L"Please place profile files (*.mfn) in folder:\n %s\\profiles", pwd);
			MessageBox(msg, L"Error mdfblocks.mfn not found");
		}
		else
		{
			msg.Format(L"Please update your profiles (*.mfn) to version %s in folder:\n %s\\profiles", ProfileVersion, pwd);
			MessageBox(msg, L"Invalid Profiles");
		}
		return 0;
	}
	return TRUE;
}

void CMDFourierGUIDlg::ManageWindows(BOOL Enable)
{
	m_ExecuteBttn.EnableWindow(Enable);
	m_ReferenceFileFileBttn.EnableWindow(Enable);
	m_ComparisonFileBttn.EnableWindow(Enable);
	m_WindowTypeSelect.EnableWindow(Enable);
	m_CurveAdjustSelect.EnableWindow(Enable);
	m_AlignFFTWCheckBox.EnableWindow(Enable);
	m_ExtraDataCheckBox.EnableWindow(Enable);
	
	m_VerboseLogCheckBox.EnableWindow(Enable);
	if(m_EnableExtraCommandCheckBox.GetCheck() == BST_CHECKED)
		m_ExtraParamsEditBox.EnableWindow(Enable);
	m_EnableExtraCommandCheckBox.EnableWindow(Enable);

	m_DifferencesCheckBox.EnableWindow(Enable);
	m_MissingExtraCheckBox.EnableWindow(Enable);
	m_SpectrogramsCheckBox.EnableWindow(Enable);
	m_NoiseFloorCheckBox.EnableWindow(Enable);
	m_TimeSpectrogramCheckBox.EnableWindow(Enable);
	m_WaveFormCheckBox.EnableWindow(Enable);
	m_PhaseCheckBox.EnableWindow(Enable);
	m_FullResTimeSpectrCheckBox.EnableWindow(Enable);
	m_AveragePlotCheckBox.EnableWindow(Enable);

	m_SwapBttn.EnableWindow(Enable);
	m_MDWaveBttn.EnableWindow(Enable);

	m_RefSync.EnableWindow(Enable);
	m_ComSync.EnableWindow(Enable);

	m_Profiles.EnableWindow(Enable);
	m_Resolution.EnableWindow(Enable);

	if(mdwave)
		m_ComparisonLbl.EnableWindow(Enable);

	if(Enable)
		m_CloseBttn.SetWindowText(L"Close");
	else
		m_CloseBttn.SetWindowText(L"Terminate");
}


void CMDFourierGUIDlg::OnBnClickedAbout()
{
	CString msg;

	msg.Format(L"MDFourier Front End\n\nArtemio Urbina 2019-2024\nUsing %s\nCode available under GPL\n\nhttp://junkerhq.net/MDFourier/\n\nOpen website and manual?", 
		MDFVersion);
	if(MessageBox(msg, L"About MDFourier", MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
	{
		ShellExecute(0, 0, L"http://junkerhq.net/MDFourier/", 0, 0 , SW_SHOW );
	}
}


void CMDFourierGUIDlg::OnBnClickedEnableextra()
{
	if(m_EnableExtraCommandCheckBox.GetCheck() == BST_CHECKED)
		m_ExtraParamsEditBox.EnableWindow(TRUE);
	else
		m_ExtraParamsEditBox.EnableWindow(FALSE);
}

void CMDFourierGUIDlg::CheckPlotSelection(CButton &clicked)
{
	int checked = 0;

	if(m_DifferencesCheckBox.GetCheck() == BST_CHECKED)
		checked ++;
	if(m_MissingExtraCheckBox.GetCheck() == BST_CHECKED)
		checked ++;
	if(m_SpectrogramsCheckBox.GetCheck() == BST_CHECKED)
		checked ++;
	if(m_NoiseFloorCheckBox.GetCheck() == BST_CHECKED)
		checked ++;
	if(m_TimeSpectrogramCheckBox.GetCheck() == BST_CHECKED)
		checked ++;
	if(m_WaveFormCheckBox.GetCheck() == BST_CHECKED)
		checked ++;
	if(m_PhaseCheckBox.GetCheck() == BST_CHECKED)
		checked ++;
	if(m_AveragePlotCheckBox.GetCheck() == BST_CHECKED)
		checked ++;

	if(!checked)
		clicked.SetCheck(TRUE);

	//m_OpenResultsBttn.EnableWindow(FALSE);
}

void CMDFourierGUIDlg::OnBnClickedDifferences()
{
	CheckPlotSelection(m_DifferencesCheckBox);
}

void CMDFourierGUIDlg::OnBnClickedMissing()
{
	CheckPlotSelection(m_MissingExtraCheckBox);
}

void CMDFourierGUIDlg::OnBnClickedSpectrogram()
{
	CheckPlotSelection(m_SpectrogramsCheckBox);
}

void CMDFourierGUIDlg::OnBnClickedAverage()
{
	CheckPlotSelection(m_AveragePlotCheckBox);
}

void CMDFourierGUIDlg::OnBnClickedNoisefloor()
{
	CheckPlotSelection(m_NoiseFloorCheckBox);
}


void CMDFourierGUIDlg::OnBnClickedTimesp()
{
	CheckPlotSelection(m_TimeSpectrogramCheckBox);
	if(m_TimeSpectrogramCheckBox.GetCheck() == BST_CHECKED)
		m_FullResTimeSpectrCheckBox.EnableWindow(TRUE);
	else
		m_FullResTimeSpectrCheckBox.EnableWindow(FALSE);
}

void CMDFourierGUIDlg::OnBnClickedPlotTd()
{
	CheckPlotSelection(m_WaveFormCheckBox);
}


void CMDFourierGUIDlg::OnBnClickedPhase()
{
	CheckPlotSelection(m_PhaseCheckBox);
}

void CMDFourierGUIDlg::OnBnClickedMdwave()
{
	CString profile;
	CString	command, extraCmd;
	CString	window, syncFormat;

	UpdateWindow();

	ClearResults();

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

	if(!m_ReferenceFile.GetLength())
	{
		MessageBox(L"Please select a Reference audio file.", L"Error");
		return;
	}

	profile = GetSelectedCommandLineValue(m_Profiles, COUNT_PROFILES);
	window = GetSelectedCommandLineValue(m_WindowTypeSelect, COUNT_WINDOWS);
	syncFormat = GetSelectedCommandLineValue(m_RefSync, syncTypes);

	command.Format(L"mdwave.exe -P \"%s\" -r \"%s\" -w %s -Y %s -c", 
			profile, m_ReferenceFile, window, syncFormat);

	if(m_AlignFFTWCheckBox.GetCheck() == BST_CHECKED)
		command += " -z";

	if(m_VerboseLogCheckBox.GetCheck() == BST_CHECKED)
	{
		command += " -v";
		cDos.verbose = TRUE;
	}
	else
		cDos.verbose = FALSE;

	if(m_EnableExtraCommandCheckBox.GetCheck() == BST_CHECKED)
		m_ExtraParamsEditBox.GetWindowText(extraCmd);
	if(extraCmd.GetLength())
	{
		command += L" ";
		command += extraCmd;
	}

	mdwave = true;
	ManageWindows(FALSE);

	SetTimer(IDT_DOS, 100, 0);

	m_OutputTextCtrl.SetWindowText(L"");
	//m_ComparisonLbl.SetWindowText(L"");
	//m_ComparisonFile = L"";

	cDos.Start(command);
}

void CMDFourierGUIDlg::OnBnClickedSwap()
{
	int pos = 0, sel = 0;
	CString tmp;

	if(!m_ReferenceFile.GetLength() && !m_ComparisonFile.GetLength())
		return;
	tmp = m_ComparisonFile;
	tmp.MakeLower();
	pos = tmp.Find(L".mfl", 0);
	if(pos != -1)
		return;
	
	tmp = m_ReferenceFile;

	m_ReferenceFile = m_ComparisonFile;
	m_ReferenceFileLbl.SetWindowText(m_ReferenceFile); 
	m_ComparisonFile = tmp;
	m_ComparisonLbl.SetWindowText(m_ComparisonFile);

	sel = m_RefSync.GetCurSel();
	m_RefSync.SetCurSel(m_ComSync.GetCurSel());
	m_ComSync.SetCurSel(sel);

	m_OpenResultsBttn.EnableWindow(FALSE);
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
						m_ReferenceFile = szDroppedFile;
						m_ReferenceFileLbl.SetWindowText(m_ReferenceFile); 
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
		if(VerifyFileExtension(szDroppedFile, m_ReferenceFile.GetLength() && !m_ComparisonFile.GetLength()))
		{
			if(m_ReferenceFile.GetLength() && m_ComparisonFile.GetLength())
			{
				m_ReferenceFile.Empty();
				m_ReferenceFileLbl.SetWindowText(m_ReferenceFile); 
				m_ComparisonFile.Empty();
				m_ComparisonLbl.SetWindowText(m_ComparisonFile);
				erase = true;
			}

			if(!m_ReferenceFile.GetLength())
			{
				m_ReferenceFile = szDroppedFile;
				m_ReferenceFileLbl.SetWindowText(m_ReferenceFile); 
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
		m_OpenResultsBttn.EnableWindow(FALSE);
	return 0;

}

void CMDFourierGUIDlg::OnCbnDropdownProfile()
{
	int matched = 0;

	matched = FindProfiles(L"profiles", L"*.mfn");
	if(matched <= 0)
	{
		CString msg;
		TCHAR	pwd[MAX_PATH];

		GetCurrentDirectory(MAX_PATH, pwd);

		if(matched == 0)
		{
			msg.Format(L"Please place profile files (*.mfn) in folder:\n %s\\profiles", pwd);
			MessageBox(msg, L"Error mdfblocks.mfn not found");
		}
		else
		{
			msg.Format(L"Please update your profiles (*.mfn) to version %s in folder:\n %s\\profiles", ProfileVersion, pwd);
			MessageBox(msg, L"Invalid Profiles");
		}
	}
}


void CMDFourierGUIDlg::OnCbnSelendokProfile()
{
	int					match = 0, syncCount = 0;
	CString				Name, Version, Error, FullFileName;
	CommandLineArray	ProfileSyncTypes[COUNT_SYNCTYPE];

	syncTypes = 0;
	m_RefSync.ResetContent();
	m_ComSync.ResetContent();
	m_OpenResultsBttn.EnableWindow(FALSE);

	FullFileName = GetSelectedCommandLineValue(m_Profiles, COUNT_PROFILES);
	if(FullFileName == L"NONE")
		return;

	match = LoadProfile(FullFileName, Name, Version, Error, ProfileSyncTypes, COUNT_SYNCTYPE, syncCount);
	if(match <= 0)
	{
		MessageBox(Error, L"Invalid Profile");
		return;
	}

	if(syncCount)
	{
		for(int i = 0; i < syncCount; i++)
		{
			InsertValueInCombo(ProfileSyncTypes[i].Name, ProfileSyncTypes[i].valueMDF, SyncType[i], m_RefSync);
			InsertValueInCombo(ProfileSyncTypes[i].Name, ProfileSyncTypes[i].valueMDF, SyncType[i], m_ComSync);
		}
		m_RefSync.SetCurSel(0);
		m_ComSync.SetCurSel(0);
		syncTypes = syncCount;
	}
	wintextProfile = Name;
	ChangeWindowText();
}


void CMDFourierGUIDlg::OnCbnSelendcancelProfile()
{
	CString FullFileName;

	FullFileName = GetSelectedCommandLineValue(m_Profiles, COUNT_PROFILES);
	if(FullFileName == L"NONE")
	{
		syncTypes = 0;
		m_RefSync.ResetContent();
		m_ComSync.ResetContent();
		m_OpenResultsBttn.EnableWindow(FALSE);
		wintextProfile.Empty();
		ChangeWindowText();
	}
}
