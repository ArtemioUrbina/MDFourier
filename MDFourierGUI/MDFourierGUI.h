
// MDFourierGUI.h : main header file for the PROJECT_NAME application
//

#pragma once

#pragma comment(lib, "windowscodecs.lib")

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CMDFourierGUIApp:
// See MDFourierGUI.cpp for the implementation of this class
//

#define BUFFER_SIZE 4096

class CMDFourierGUIApp : public CWinApp
{
public:
	CMDFourierGUIApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CMDFourierGUIApp theApp;