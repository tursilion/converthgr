// TIPicView.h : main header file for the TIPICVIEW application
//

#if !defined(AFX_TIPICVIEW_H__F4725884_9A29_4492_AE12_0E3B106C2738__INCLUDED_)
#define AFX_TIPICVIEW_H__F4725884_9A29_4492_AE12_0E3B106C2738__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

#define XOFFSET 250

typedef struct tagMYRGBQUAD {
	// mine is RGB instead of BGR
        BYTE    rgbRed;
        BYTE    rgbGreen;
        BYTE    rgbBlue;
        BYTE    rgbReserved;
} MYRGBQUAD;

/////////////////////////////////////////////////////////////////////////////
// CTIPicViewApp:
// See TIPicView.cpp for the implementation of this class
//

class CTIPicViewApp : public CWinApp
{
public:
	CTIPicViewApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTIPicViewApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation
	void loadSettings();
	void saveSettings();

	//{{AFX_MSG(CTIPicViewApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

BOOL MYRGBTo8BitDithered(BYTE *pRGB, UINT32 uWidth, UINT32 uHeight, BYTE *p8Bit, UINT32 uColors, MYRGBQUAD *pal);
void debug(char *s, ...);

// threshold map for ordered dither
extern double g_thresholdMap[4][4];
extern double g_thresholdMap2[4][4];

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

double yuvpaldist(double r1, double g1, double b1, int nCol);
void makeYUV(double r, double g, double b, double &y, double &u, double &v);
void makeY(double r, double g, double b, double &y);

#endif // !defined(AFX_TIPICVIEW_H__F4725884_9A29_4492_AE12_0E3B106C2738__INCLUDED_)

