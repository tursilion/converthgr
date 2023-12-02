// TIPicView.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <stdio.h>
#include <math.h>
#include <process.h>
#include "D:\WORK\imgsource\4.0\islibs40_vs17_unicode\ISource.h"
#include "D:\WORK\imgsource\2.1\src\ISLib\isarray.h"
#include <windows.h>
#include <list>
#include <cmath>
#include "median_cut.h"

#pragma pack(1)

#include "TIPicView.h"
#include "TIPicViewDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


uchar pal[256][4];	// RGB0
double YUVpal[256][4];	// YCrCb0 - misleadingly called YUV for easier typing
int pixels[40][8][4];
extern MYRGBQUAD palinit16[256], defaultpalinit16[16];
Point points[280];

// hacky command line interface
wchar_t cmdLineCopy[1024];
wchar_t *cmdFileOut, *cmdFileIn;
#define MAX_OPTIONS 128
wchar_t *cmdOptions[MAX_OPTIONS];
int numOptions;

int nCurrentByte;
int PIXA,PIXB,PIXC,PIXD,PIXE,PIXF;
int g_orderSlide = 0;			// the step (n/17) that is subtracted from the ordered dither pattern to darken
int g_nFilter=4;
int g_nPortraitMode=0;
int g_Perceptual=0;				// enable perceptual RGB color matching instead of YCrCb
int g_AccumulateErrors=1;		// accumulate errors instead of averaging (old method)
int g_MaxColDiff=1;				// max color shift allowed (>15 is pretty pointless - percentage of color space)
int g_OrderedDither=0;			// whether to use the built-in ordered dither 1 or 2
int g_MapSize=2;				// whether ordered dither uses 2x2 or 4x4 maps
double g_PercepR=0.30, g_PercepG=0.52, g_PercepB=0.18;
double g_LumaEmphasis = 0.8;
double g_Gamma = 1.3;
bool g_Grey = false;

extern unsigned char buf8[280*192];
extern RGBQUAD winpal[256];
extern CWnd *pWnd;
extern bool StretchHist;
extern int pixeloffset;
extern int heightoffset;
extern int ScaleMode;
extern bool fVerbose;


void quantize_new(BYTE* pRGB, BYTE* p8Bit, double dark, int mapsize);
void quantize_new_percept(BYTE* pRGB, BYTE* p8Bit, double dark, int mapsize);
void quantize_new_ordered(BYTE* pRGB, BYTE* p8Bit, double dark, int mapsize);
void quantize_new_percept_ordered(BYTE* pRGB, BYTE* p8Bit, double dark, int mapsize);
void quantize_new_ordered2(BYTE* pRGB, BYTE* p8Bit, double dark, int mapsize);
void quantize_new_percept_ordered2(BYTE* pRGB, BYTE* p8Bit, double dark, int mapsize);
/////////////////////////////////////////////////////////////////////////////

// we manually wrap the Win10 function GetDpiForWindow()
// if it's not available, we just return the default of 96
UINT WINAPI GetDpiForWindow(_In_ HWND hWnd) {
	static UINT (WINAPI *getDpi)(_In_ HWND) = NULL;
	static bool didSearch = false;

	if (!didSearch) {
		didSearch = true;
		HMODULE hMod = LoadLibrary(_T("user32.dll"));
		if (NULL == hMod) {
			printf("Failed to load user32.dll (what? really??)\n");
		} else {
			getDpi = (UINT (WINAPI *)(_In_ HWND))GetProcAddress(hMod, "GetDpiForWindow");
			if (NULL == getDpi) {
				printf("Failed to find GetDpiForWindow, defaulting to 96dpi\n");
			} else {
				if (fVerbose) {
					printf("Got GetDpiForWindow, should be able to auto-scale.\n");
				}
			}
		}
	}

	if (NULL == getDpi) {
		return 96;
	} else {
		return getDpi(hWnd);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CTIPicViewApp

BEGIN_MESSAGE_MAP(CTIPicViewApp, CWinApp)
	//{{AFX_MSG_MAP(CTIPicViewApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTIPicViewApp construction

CTIPicViewApp::CTIPicViewApp()
{
	// add construction code here,
	// Place all significant initialization in InitInstance
}

#define INIFILE _T("convert9918.ini")
wchar_t INIFILEPATH[1024];

// helpers
void writeint(int n, wchar_t *name) {
	wchar_t buf[32];
	_swprintf(buf, _T("%d"), n);
	WritePrivateProfileString(_T("ConvertHGR"), name, buf, INIFILEPATH);
}

void writefloat(double n, wchar_t *name) {
	wchar_t buf[64];
	int x = (int)(n*1000);
	_swprintf(buf, _T("%d"), x);
	WritePrivateProfileString(_T("ConvertHGR"), name, buf, INIFILEPATH);
}

void writebool(bool n, wchar_t *name) {
	if (n) {
		WritePrivateProfileString(_T("ConvertHGR"), name, _T("1"), INIFILEPATH);
	} else {
		WritePrivateProfileString(_T("ConvertHGR"), name, _T("0"), INIFILEPATH);
	}
}

void writequad(MYRGBQUAD n, wchar_t *name, wchar_t *key) {
	wchar_t buf[64];
	_swprintf(buf, _T("%d,%d,%d,%d"), n.rgbRed, n.rgbGreen, n.rgbBlue, n.rgbReserved);
	WritePrivateProfileString(key, name, buf, INIFILEPATH);
}

void profileString(wchar_t *group, wchar_t *key, wchar_t *defval, wchar_t *outbuf, int outsize, wchar_t *path) {
	// wraps GetPrivateProfileString and also checks the command line for easy override
	// note that the command line ignores group, so no duplicate key names are allowed
	// in the INI, groups there are really just for human readability.
	// note that regardless of command line or not, debug is only emitted in this function
	// when fVerbose is set.

	wchar_t workbuf[128];
	bool bFound = false;

	// apply default value
	wcsncpy(outbuf, defval, outsize);
	outbuf[outsize-1]='\0';

	// check command line for key
	_snwprintf(workbuf, 128, _T("/%s="), key);
	workbuf[127]=_T('\0');

	for (int idx=0; idx<numOptions; idx++) {
		if (0 == wcsncmp(cmdOptions[idx], workbuf, wcslen(workbuf))) {
			// copy out just the data part - we know where the equals sign is!
			wcsncpy(outbuf, &cmdOptions[idx][wcslen(workbuf)], outsize);
			outbuf[outsize-1]=_T('\0');
			if (fVerbose) {
				debug(_T("%s/%s = %s (cmdline)\n"), group, key, outbuf);
			}
			bFound = true;
			break;
		}
	}

	if (!bFound) {
		// usual case, read from the INI instead
		GetPrivateProfileString(group, key, defval, outbuf, outsize, path);
		if (fVerbose) {
			debug(_T("%s/%s = %s\n"), group, key, outbuf);
		}
	}
}

int profileInt(wchar_t *group, wchar_t *key, int defval, wchar_t *path) {
	// return value as int
	wchar_t buf[64], bufout[64];
	swprintf(buf, 64, _T("%d"), defval);

	profileString(group, key, buf, bufout, 64, path);
	return _wtoi(bufout);
}

void readint(int &n, wchar_t *key) {
	int x;
	x = profileInt(_T("ConvertHGR"), key, -9941, INIFILEPATH);
	if (x != -9941) n=x;
}

void readfloat(double &n, wchar_t *key) {
	int x;
	x = profileInt(_T("ConvertHGR"), key, -9941, INIFILEPATH);
	if (x != -9941) {
		n = (double)x / 1000.0;
	}
}

void readbool(bool &n, wchar_t *key) {
	int x;
	x = profileInt(_T("ConvertHGR"), key, -9941, INIFILEPATH);
	if (x != -9941) {
		if (x) {
			n = true;
		} else {
			n = false;
		}
	}
}

void readquad(MYRGBQUAD &n, wchar_t *key, wchar_t *group) {
	wchar_t buf[64];
	int r,g,b,a;

	profileString(group, key, _T(""), buf, 64, INIFILEPATH);
	if (buf[0] == '\0') return;
	if (4 != swscanf(buf, _T("%d,%d,%d,%d"), &r, &g, &b, &a)) return;
	n.rgbRed=r&0xff;
	n.rgbGreen=g&0xff;
	n.rgbBlue=b&0xff;
	n.rgbReserved=a&0xff;
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CTIPicViewApp object

CTIPicViewApp theApp;

// settings - just load/save off the globals
void CTIPicViewApp::loadSettings() {
	if (0 == GetCurrentDirectory(sizeof(INIFILEPATH), INIFILEPATH)) {
		wcscpy(INIFILEPATH, _T(".\\") INIFILE);
	} else {
		wcscat(INIFILEPATH, _T("\\"));
		wcscat(INIFILEPATH, INIFILE);
	}

	readint(PIXA,               _T("PIXA"));
	readint(PIXB,               _T("PIXB"));
	readint(PIXC,               _T("PIXC"));
	readint(PIXD,               _T("PIXD"));
	readint(PIXE,               _T("PIXE"));
	readint(PIXF,               _T("PIXF"));
	readint(g_orderSlide,       _T("OrderSlide"));
	readint(g_nFilter,          _T("Filter"));
	readint(g_nPortraitMode,    _T("PortraitMode"));
	readint(g_Perceptual,       _T("Perceptual"));
	readint(g_AccumulateErrors, _T("AccumulateErrors"));
	readint(g_MaxColDiff,       _T("MaxColDiff"));
	readint(g_OrderedDither,    _T("OrderedDither"));
	readint(g_MapSize,          _T("MapSize"));
	if ((g_MapSize!=2)&&(g_MapSize!=4)) g_MapSize=2;
	readfloat(g_PercepR,        _T("PerceptR"));
	readfloat(g_PercepG,        _T("PerceptG"));
	readfloat(g_PercepB,        _T("PerceptB"));
	readfloat(g_LumaEmphasis,   _T("LumaEmphasis"));
	readfloat(g_Gamma,          _T("Gamma"));
	readbool(StretchHist,       _T("StretchHistogram"));
	readint(pixeloffset,        _T("PixelOffset"));
	readint(heightoffset,       _T("HeightOffset"));
	readint(ScaleMode,          _T("ScaleMode"));
	readquad(palinit16[0], 		_T("black"), 	_T("palette"));
	readquad(palinit16[1], 		_T("green"), 	_T("palette"));
	readquad(palinit16[2], 		_T("purple"), 	_T("palette"));
	readquad(palinit16[3], 		_T("white"), 	_T("palette"));
	readquad(palinit16[4], 		_T("black2"), 	_T("palette"));
	readquad(palinit16[5], 		_T("orange"), 	_T("palette"));
	readquad(palinit16[6], 		_T("blue"), 	_T("palette"));
	readquad(palinit16[7], 		_T("white2"), 	_T("palette"));
}

void CTIPicViewApp::saveSettings() {
	SetCurrentDirectory(INIFILEPATH);

	writeint(PIXA,                  _T("PIXA"));
	writeint(PIXB,                  _T("PIXB"));
	writeint(PIXC,                  _T("PIXC"));
	writeint(PIXD,                  _T("PIXD"));
	writeint(PIXE,                  _T("PIXE"));
	writeint(PIXF,                  _T("PIXF"));
	writeint(g_orderSlide,          _T("OrderSlide"));
	writeint(g_nFilter,             _T("Filter"));
	writeint(g_nPortraitMode,       _T("PortraitMode"));
	writeint(g_Perceptual,          _T("Perceptual"));
	writeint(g_AccumulateErrors,    _T("AccumulateErrors"));
	writeint(g_MaxColDiff,          _T("MaxColDiff"));
	writeint(g_OrderedDither,       _T("OrderedDither"));
	writeint(g_MapSize,             _T("MapSize"));
	writefloat(g_PercepR,           _T("PerceptR"));
	writefloat(g_PercepG,           _T("PerceptG"));
	writefloat(g_PercepB,           _T("PerceptB"));
	writefloat(g_LumaEmphasis,      _T("LumaEmphasis"));
	writefloat(g_Gamma,             _T("Gamma"));
	writebool(StretchHist,          _T("StretchHistogram"));
	writeint(pixeloffset,           _T("PixelOffset"));
	writeint(heightoffset,          _T("HeightOffset"));
	writeint(ScaleMode,             _T("ScaleMode"));
	writequad(palinit16[0], 		_T("black"), 	_T("palette"));
	writequad(palinit16[1], 		_T("green"), 	_T("palette"));
	writequad(palinit16[2], 		_T("purple"), 	_T("palette"));
	writequad(palinit16[3], 		_T("white"), 	_T("palette"));
	writequad(palinit16[4], 		_T("black2"), 	_T("palette"));
	writequad(palinit16[5], 		_T("orange"), 	_T("palette"));
	writequad(palinit16[6], 		_T("blue"), 	_T("palette"));
	writequad(palinit16[7], 		_T("white2"),	_T("palette"));
	// no duplicate key names are allowed in this app due to command line overrides
	writequad(defaultpalinit16[0],  _T("def_black"),	_T("default_palette"));
	writequad(defaultpalinit16[1],  _T("def_green"), 	_T("default_palette"));
	writequad(defaultpalinit16[2],  _T("def_purple"), 	_T("default_palette"));
	writequad(defaultpalinit16[3],  _T("def_white"), 	_T("default_palette"));
	writequad(defaultpalinit16[4],  _T("def_black2"), 	_T("default_palette"));
	writequad(defaultpalinit16[5],  _T("def_orange"), 	_T("default_palette"));
	writequad(defaultpalinit16[6],  _T("def_blue"), 	_T("default_palette"));
	writequad(defaultpalinit16[7],  _T("def_white2"), 	_T("default_palette"));
}

void GetConsole() {
	static bool gotIt = false;	// only once per app

	if (gotIt) return;
	gotIt=true;

	if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
		// create a console for debugging
		AllocConsole();
	}

#if 1
    freopen("CONOUT$", "w", stdout);
#else
	int hCrt, i;
	FILE *hf;
	hCrt = _open_osfhandle((long) GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
	hf = _fdopen( hCrt, "w" );
	*stdout = *hf;
	i = setvbuf( stdout, NULL, _IONBF, 0 ); 
#endif
}

/////////////////////////////////////////////////////////////////////////////
// CTIPicViewApp initialization

BOOL CTIPicViewApp::InitInstance()
{
	// command line - most behaviour is determined by cmdFileXxx
	// cmdOptions just stores all the /Option=x string pointers
	// cmdXXX vars are just pointers into cmdLineCopy, which has
	// spaces replaced with NULs.
	cmdFileOut = NULL;
	cmdFileIn = NULL;
	memset(cmdOptions, 0, sizeof(cmdOptions));
	numOptions = 0;

	// we might already have it depending on the command line
	GetConsole();

	// really shouldn't parse this ourselves, but it's late and I'm tired.
	// and now I build upon that evil! Muhaha! It's not even late this time!
	int tokenStart = -1;
	int idx=0;
	bool quote=false;
	if (m_lpCmdLine[0] != '\0') {
		wcsncpy(cmdLineCopy, m_lpCmdLine, sizeof(cmdLineCopy));
		cmdLineCopy[sizeof(cmdLineCopy)-1] = _T('\0');
		if (wcslen(cmdLineCopy) < sizeof(cmdLineCopy)-2) {
			// pad with a space so we parse the last option
			wcscat(cmdLineCopy, _T(" "));
		}

		while (cmdLineCopy[idx] != _T('\0')) {
			// handle escaped characters and quoted strings
			if (cmdLineCopy[idx] == _T('\\')) {
				idx++;
				if (cmdLineCopy[idx] != _T('\0')) idx++;
				continue;
			}
			if (cmdLineCopy[idx] == _T('\"')) {
				quote=!quote;
				++idx;
				continue;
			}
			if ((cmdLineCopy[idx] == _T(' ')) && (!quote)) {
				cmdLineCopy[idx] = _T('\0');
				if (tokenStart != -1) {
					// we had a string, what was it?
					if (cmdLineCopy[tokenStart] == _T('/')) {
						// as an aside, check immediates here
						if (0 == wcscmp(_T("/verbose"), &cmdLineCopy[tokenStart])) {
							GetConsole();	// open the console now
							fVerbose = true;
							debug(_T("Verbose mode on"));
						} else if (0 == wcscmp(_T("/?"), &cmdLineCopy[tokenStart])) {
							printf("\nconverthgr [options] input output\n");
							printf(" /verbose = enable verbose output\n");
							printf(" Any other option from the INI may be specified as \"/option=value\" - see INI.\n");
							ExitProcess(0);
						} else if (numOptions < MAX_OPTIONS) {
							// now save off the option in the array
							cmdOptions[numOptions++] = &cmdLineCopy[tokenStart];
						} else {
							printf("** Error, more than %d options can not be read.\n", MAX_OPTIONS);
							ExitProcess(-10);
						}
					} else {
						// should be one of the filenames - in then out
						if (NULL == cmdFileIn) {
							cmdFileIn = &cmdLineCopy[tokenStart];
						} else if (NULL == cmdFileOut) {
							cmdFileOut = &cmdLineCopy[tokenStart];
						} else {
							printf("** Error - too many filenames on command line.\n");
							ExitProcess(-10);
						}
					}
					tokenStart = -1;
				}
			}
			if ((cmdLineCopy[idx]!=_T('\0'))&&(!isspace(cmdLineCopy[idx]))&&(tokenStart == -1)) {
				tokenStart = idx;
			}
			++idx;
		}
		// printf can handle NULL, so this is okay
		printf("File In: %S\nFileOut: %S\n", cmdFileIn, cmdFileOut);
	}

	// bring up the dialog
	CTIPicViewDlg dlg;
	m_pMainWnd = &dlg;

	// backup palinit16 before we load settings
	memcpy(defaultpalinit16, palinit16, sizeof(RGBQUAD)*16);

	// load settings (IF there is an INI)
	loadSettings();

	// don't care about the return code
	dlg.DoModal();

	// save settings (unless we had command line parameters)
	if (numOptions == 0) {
		saveSettings();
	} else {
		if (fVerbose) {
			debug(_T("Skipping saving settings due to command line parameters."));
		}
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}


// pRGB - input image - 280x192x24-bit image
// p8Bit - output image - 280x192x8-bit image, we will provide palette
// pal - palette to use (8-color (really 6) Apple2e palette
// many global flags override
void MYRGBTo8BitDithered(BYTE *pRGB, BYTE *p8Bit, MYRGBQUAD *inpal, double darken)
{
	// prepare palette (this will be overridden later if needed)
	for (int i=0; i<8; i++) {
		pal[i][0]=inpal[i].rgbRed;
		pal[i][1]=inpal[i].rgbGreen;
		pal[i][2]=inpal[i].rgbBlue;

		makeYUV(pal[i][0], pal[i][1], pal[i][2], YUVpal[i][0], YUVpal[i][1], YUVpal[i][2]);
	}

	// this function handles it all
	AfxGetMainWnd()->EnableWindow(FALSE);
	if (g_Perceptual) {
		if (g_OrderedDither == 2) {
			quantize_new_percept_ordered2(pRGB, p8Bit, darken, g_MapSize);
		} else if (g_OrderedDither) {
			quantize_new_percept_ordered(pRGB, p8Bit, darken, g_MapSize);
		} else {
			quantize_new_percept(pRGB, p8Bit, darken, g_MapSize);
		}
	} else {
		if (g_OrderedDither == 2) {
			quantize_new_ordered2(pRGB, p8Bit, darken, g_MapSize);
		} else if (g_OrderedDither) {
			quantize_new_ordered(pRGB, p8Bit, darken, g_MapSize);
		} else {
			quantize_new(pRGB, p8Bit, darken, g_MapSize);
		}
	}
	AfxGetMainWnd()->EnableWindow(TRUE);
}

