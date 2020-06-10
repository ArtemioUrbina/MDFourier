#pragma once
#include "afxwin.h"


// CWarnings dialog

class CWarnings : public CDialogEx
{
	DECLARE_DYNAMIC(CWarnings)

public:
	CWarnings(CWnd* pParent = NULL);   // standard constructor
	virtual ~CWarnings();

// Dialog Data
	enum { IDD = IDD_WARNINGS };

protected:
	bool SetError;
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	void SetWarnings(CString warnings, bool isError = false) { m_WarningsText = warnings; if(isError) SetError = true;}
	CString m_WarningsText;
	virtual BOOL OnInitDialog();
};
