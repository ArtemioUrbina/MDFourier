// Warnings.cpp : implementation file
//

#include "stdafx.h"
#include "MDFourierGUI.h"
#include "Warnings.h"
#include "afxdialogex.h"


// CWarnings dialog

IMPLEMENT_DYNAMIC(CWarnings, CDialogEx)

CWarnings::CWarnings(CWnd* pParent /*=NULL*/)
	: CDialogEx(CWarnings::IDD, pParent)
	, m_WarningsText(_T(""))
{
	SetError = false;
}

CWarnings::~CWarnings()
{
}

void CWarnings::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_OUTPUT, m_WarningsText);
}


BEGIN_MESSAGE_MAP(CWarnings, CDialogEx)
END_MESSAGE_MAP()


// CWarnings message handlers


BOOL CWarnings::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	if(SetError)
		SetWindowText(L"Errors from MDFourier");

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}
