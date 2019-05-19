#pragma once

#define BUFSIZE 4096

class CDOSExecute
{
public:
	CDOSExecute(void);
	~CDOSExecute(void);

	int ExecuteExternalFile();

	CString			m_cline;
    CWinThread*		m_pcThread;
	BOOL			m_fAbortNow;
	BOOL			m_fDone;
	CString			m_Output;
	HANDLE			Semaphore;

    static UINT ExecuteExternalFileThreadProc( LPVOID pParam ) {
        CDOSExecute* pThis= (CDOSExecute*)pParam;
        UINT nRet= pThis->ExecuteExternalFile();     // get out of 'static mode'
        return( nRet );
    }
	void Start( CString cline ) {
		m_cline = cline;
        m_fAbortNow = FALSE;
        m_pcThread = AfxBeginThread( ExecuteExternalFileThreadProc, this );
    }
    void StopNow(void) {
        m_fAbortNow= TRUE;
	}

	void KillNow(void) {
		HANDLE hThread = m_pcThread->m_hThread;
		DWORD nExitCode;
		BOOL fRet= ::GetExitCodeThread(hThread, &nExitCode);

		if ( fRet && nExitCode==STILL_ACTIVE ) { // did not die!
			TerminateThread( hThread, -1 ); // <<== Kill it (but not cleanly)
		}
	}

	int	Lock() {
		if(WaitForSingleObject(Semaphore, INFINITE) == WAIT_FAILED)
		{
			return(FALSE);
		}
		return TRUE;
	}
	int	Release() {
		if(!ReleaseSemaphore(Semaphore, 1, NULL))
		{
			return(FALSE);
		}
	
	return(TRUE);
	}
};

