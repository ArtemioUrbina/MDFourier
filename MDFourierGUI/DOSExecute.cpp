#include "StdAfx.h"
#include "DOSExecute.h"


CDOSExecute::CDOSExecute(void)
{
	SECURITY_ATTRIBUTES		Security;
	SECURITY_DESCRIPTOR		Descriptor;

	m_fAbortNow = FALSE;
	m_fDone = TRUE;
	verbose = FALSE;
	m_pcThread = NULL;

	memset(&Security, 0, sizeof(SECURITY_ATTRIBUTES));
	memset(&Descriptor, 0, sizeof(SECURITY_DESCRIPTOR));

	Semaphore = NULL;

	if(!InitializeSecurityDescriptor(&Descriptor, SECURITY_DESCRIPTOR_REVISION))
		return;

	Security.nLength = sizeof(SECURITY_ATTRIBUTES);
	Security.lpSecurityDescriptor = &Descriptor;
	Security.bInheritHandle  = TRUE;

	Semaphore = CreateSemaphore(&Security, 1, 1, L"MDFourier");
	
	DWORD Error = GetLastError();
	if(Error == ERROR_ALREADY_EXISTS)
		return;
}


CDOSExecute::~CDOSExecute(void)
{
	if(!m_fDone)
		m_fAbortNow = TRUE;	
}

int CDOSExecute::ExecuteExternalFile()
{
	int nStrBuffer;
	BOOL result;
	HANDLE rPipe, wPipe;
	char buf[BUFSIZE];
	DWORD reDword; 
	CString csTemp;
	STARTUPINFO sInfo; 
	SECURITY_ATTRIBUTES secattr; 
	COMMTIMEOUTS t = { 200, 2, 200, 2, 200 };

	Lock();
	m_fDone = FALSE;
	m_OutputText = L"";
	if(verbose)
	{
		m_OutputText += m_cline;
		m_OutputText += L"\r\n\r\n";
	}
	Release();

	ZeroMemory(&secattr,sizeof(secattr));
	secattr.nLength = sizeof(secattr);
	secattr.bInheritHandle = TRUE;

	//Create pipes to write and read data
	if(!CreatePipe(&rPipe ,&wPipe, &secattr, 100))
		return 0;

	if(!SetHandleInformation(rPipe, HANDLE_FLAG_INHERIT, 0) )
		return 0;

	ZeroMemory(&sInfo, sizeof(sInfo));
	ZeroMemory(&pInfo, sizeof(pInfo));

	sInfo.cb = sizeof(sInfo);
	sInfo.dwFlags = STARTF_USESTDHANDLES;
	sInfo.hStdInput = NULL; 
	sInfo.hStdOutput = wPipe; 
	sInfo.hStdError = wPipe;

	nStrBuffer = m_cline.GetLength() + 50;

	//Create the process here.
	result = CreateProcess(NULL, m_cline.GetBuffer(nStrBuffer), NULL, NULL, 
							TRUE, NORMAL_PRIORITY_CLASS|CREATE_NO_WINDOW, NULL, NULL, &sInfo, &pInfo);
	m_cline.ReleaseBuffer();
	CloseHandle(wPipe);

	if(!result)
	{
		// CreateProcess() failed
		// Get the error from the system
		LPVOID lpMsgBuf;
		DWORD dw = GetLastError();
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
					NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);

		// Display the error
		CString strError = (LPTSTR) lpMsgBuf;
		TRACE(_T("::executeCommandLine() failed at CreateProcess()\nCommand=%s\nMessage=%s\n\n"), m_cline, strError);

		// Free resources created by the system
		LocalFree(lpMsgBuf);
		m_fDone = TRUE;
		return 0;
	}

	//now read the output pipe here.
	do
	{
		result = ::ReadFile(rPipe, buf, BUFSIZE, &reDword, NULL);
		if(result)
		{
			csTemp = buf;
			Lock();
			m_OutputText += csTemp.Left(reDword);
			Release();

			if(m_fAbortNow && pInfo.hProcess != NULL)
			{
				TerminateProcess(pInfo.hProcess, -1);
				result = FALSE;

				Lock();
				m_OutputText = L"Terminating process. Please wait...";
				Release();
			}	
		}
	}while(result);

	CloseHandle(pInfo.hProcess);
	CloseHandle(pInfo.hThread);
	CloseHandle(rPipe);

	m_fDone = TRUE;

	return 1;
}