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
#include "C:\WORK\imgsource\4.0\islibs40_vs05\ISource.h"
#include "C:\WORK\imgsource\2.1\src\ISLib\isarray.h"
#include <windows.h>
#include <list>

#pragma pack(1)

#include "TIPicView.h"
#include "TIPicViewDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

typedef unsigned int UINT32;
typedef unsigned char uchar;
typedef unsigned long ulong;
typedef unsigned long slong;
typedef short sshort;
typedef char schar;

static uchar pal[256][4];	// RGB0
static double YUVpal[256][4];	// YCrCb0 - misleadingly called YUV for easier typing
int pixels[40][8][4];
extern MYRGBQUAD palinit16[256], defaultpalinit16[16];

// hacky command line interface
char cmdLineCopy[1024];
char *cmdFileOut, *cmdFileIn;
#define MAX_OPTIONS 128
char *cmdOptions[MAX_OPTIONS];
int numOptions;

int nCurrentByte;
int PIXA,PIXB,PIXC,PIXD,PIXE,PIXF;
int g_nFilter=4;
int g_nPortraitMode=0;
int g_Perceptual=0;				// enable perceptual color matching instead of mathematical
int g_AccumulateErrors=1;		// accumulate errors instead of averaging (old method)
int g_MaxColDiff=2;				// max color shift allowed (>15 is pretty pointless - percentage of color space)
int g_OrderedDither=0;			// whether to use the built-in ordered dither 1 or 2
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
extern int g_OrderedDither;
extern double g_thresholdMap[4][4];
extern double g_thresholdMap2[4][4];

void quantize_new(BYTE* pRGB, BYTE* p8Bit);
void quantize_new_ordered(BYTE* pRGB, BYTE* p8Bit);
void quantize_new_percep(BYTE* pRGB, BYTE* p8Bit);
void quantize_new_ordered_percep(BYTE* pRGB, BYTE* p8Bit);

double yuvpaldist(double r1, double g1, double b1);

void makeYUV(double r, double g, double b, double &y, double &u, double &v) {
	// NOTE: I'm only using this for distance tests, so I do not add the offsets. Y should be +16, and the others +128, for 0-255 ranges.
	// also, due to error values, we often get out of range inputs, so don't assume :)

	// TODO: this function is really expensive. 9 multiplies takes around 10 seconds.. when 
	// I tested with 5 multiples instead, it dropped to 7 seconds. (a dumb test with
	// no multiplies took 8 seconds, so wtf? ;) )

	y = 0.299*r + 0.587*g + 0.114*b;
	u = 0.500*r - 0.419*g - 0.081*b;
	v =-0.169*r - 0.331*g + 0.500*b;
}

// does a Y-only conversion from in (RGB) to out (Y)
// CHANGES out!
void makeY(double r, double g, double b, double &y) {
	y = 0.299*r + 0.587*g + 0.114*b;
}

// returns the distance squared against yuvpal
double yuvpaldist(double r, double g, double b, int nCol) {
	double y,cr,cb;
	double t1,t2,t3;

	// make YCrCb
	makeYUV(r, g, b, y, cr, cb);

	// gets diffs - y (brightness) is exaggerated to reduce noise (particularly with grey and white)
	t1=(y-YUVpal[nCol][0])*g_LumaEmphasis;	// hand tuned value
	t2=cr-YUVpal[nCol][1];
	t3=cb-YUVpal[nCol][2];

	return (t1*t1)+(t2*t2)+(t3*t3);
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

#define INIFILE "convert9918.ini"
char INIFILEPATH[1024];

// helpers
void writeint(int n, char *name) {
	char buf[32];
	sprintf(buf, "%d", n);
	WritePrivateProfileString("Convert9918", name, buf, INIFILEPATH);
}

void writefloat(double n, char *name) {
	char buf[64];
	int x = (int)(n*1000);
	sprintf(buf, "%d", x);
	WritePrivateProfileString("Convert9918", name, buf, INIFILEPATH);
}

void writebool(bool n, char *name) {
	if (n) {
		WritePrivateProfileString("Convert9918", name, "1", INIFILEPATH);
	} else {
		WritePrivateProfileString("Convert9918", name, "0", INIFILEPATH);
	}
}

void writequad(MYRGBQUAD n, char *name, char *key) {
	char buf[64];
	sprintf(buf, "%d,%d,%d,%d", n.rgbRed, n.rgbGreen, n.rgbBlue, n.rgbReserved);
	WritePrivateProfileString(key, name, buf, INIFILEPATH);
}

void profileString(char *group, char *key, char *defval, char *outbuf, int outsize, char *path) {
	// wraps GetPrivateProfileString and also checks the command line for easy override
	// note that the command line ignores group, so no duplicate key names are allowed
	// in the INI, groups there are really just for human readability.
	// note that regardless of command line or not, debug is only emitted in this function
	// when fVerbose is set.

	char workbuf[128];
	bool bFound = false;

	// apply default value
	strncpy(outbuf, defval, outsize);
	outbuf[outsize-1]='\0';

	// check command line for key
	_snprintf(workbuf, 128, "/%s=", key);
	workbuf[127]='\0';

	for (int idx=0; idx<numOptions; idx++) {
		if (0 == strncmp(cmdOptions[idx], workbuf, strlen(workbuf))) {
			// copy out just the data part - we know where the equals sign is!
			strncpy(outbuf, &cmdOptions[idx][strlen(workbuf)], outsize);
			outbuf[outsize-1]='\0';
			if (fVerbose) {
				debug("%s/%s = %s (cmdline)\n", group, key, outbuf);
			}
			bFound = true;
			break;
		}
	}

	if (!bFound) {
		// usual case, read from the INI instead
		GetPrivateProfileString(group, key, defval, outbuf, outsize, path);
		if (fVerbose) {
			debug("%s/%s = %s\n", group, key, outbuf);
		}
	}
}

int profileInt(char *group, char *key, int defval, char *path) {
	// return value as int
	char buf[64], bufout[64];
	sprintf(buf, "%d", defval);

	profileString(group, key, buf, bufout, 64, path);
	return atoi(bufout);
}

void readint(int &n, char *key) {
	int x;
	x = profileInt("Convert9918", key, -9941, INIFILEPATH);
	if (x != -9941) n=x;
}

void readfloat(double &n, char *key) {
	int x;
	x = profileInt("Convert9918", key, -9941, INIFILEPATH);
	if (x != -9941) {
		n = (double)x / 1000.0;
	}
}

void readbool(bool &n, char *key) {
	int x;
	x = profileInt("Convert9918", key, -9941, INIFILEPATH);
	if (x != -9941) {
		if (x) {
			n = true;
		} else {
			n = false;
		}
	}
}

void readquad(MYRGBQUAD &n, char *key, char *group) {
	char buf[64];
	int r,g,b,a;

	profileString(group, key, "", buf, 64, INIFILEPATH);
	if (buf[0] == '\0') return;
	if (4 != sscanf(buf, "%d,%d,%d,%d", &r, &g, &b, &a)) return;
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
		strcpy(INIFILEPATH, ".\\" INIFILE);
	} else {
		strcat(INIFILEPATH, "\\");
		strcat(INIFILEPATH, INIFILE);
	}

	readint(PIXA, "PIXA");
	readint(PIXB, "PIXB");
	readint(PIXC, "PIXC");
	readint(PIXD, "PIXD");
	readint(PIXE, "PIXE");
	readint(PIXF, "PIXF");
	readint(g_nFilter, "Filter");
	readint(g_nPortraitMode, "PortraitMode");
	readint(g_Perceptual, "Perceptual");
	readint(g_AccumulateErrors, "AccumulateErrors");
	readint(g_MaxColDiff, "MaxColDiff");
	readint(g_OrderedDither, "OrderedDither");
	readfloat(g_PercepR, "PerceptR");
	readfloat(g_PercepG, "PerceptG");
	readfloat(g_PercepB, "PerceptB");
	readfloat(g_LumaEmphasis, "LumaEmphasis");
	readfloat(g_Gamma, "Gamma");
	readbool(StretchHist, "StretchHistogram");
	readint(pixeloffset, "PixelOffset");
	readint(heightoffset, "HeightOffset");
	readint(ScaleMode, "ScaleMode");
	readquad(palinit16[0], "black", "palette");
	readquad(palinit16[1], "green", "palette");
	readquad(palinit16[2], "purple", "palette");
	readquad(palinit16[3], "white", "palette");
	readquad(palinit16[4], "black2", "palette");
	readquad(palinit16[5], "orange", "palette");
	readquad(palinit16[6], "blue", "palette");
	readquad(palinit16[7], "white2", "palette");
}

void CTIPicViewApp::saveSettings() {
	SetCurrentDirectory(INIFILEPATH);

	writeint(PIXA, "PIXA");
	writeint(PIXB, "PIXB");
	writeint(PIXC, "PIXC");
	writeint(PIXD, "PIXD");
	writeint(PIXE, "PIXE");
	writeint(PIXF, "PIXF");
	writeint(g_nFilter, "Filter");
	writeint(g_nPortraitMode, "PortraitMode");
	writeint(g_Perceptual, "Perceptual");
	writeint(g_AccumulateErrors, "AccumulateErrors");
	writeint(g_MaxColDiff, "MaxColDiff");
	writeint(g_OrderedDither, "OrderedDither");
	writefloat(g_PercepR, "PerceptR");
	writefloat(g_PercepG, "PerceptG");
	writefloat(g_PercepB, "PerceptB");
	writefloat(g_LumaEmphasis, "LumaEmphasis");
	writefloat(g_Gamma, "Gamma");
	writebool(StretchHist, "StretchHistogram");
	writeint(pixeloffset, "PixelOffset");
	writeint(heightoffset, "HeightOffset");
	writeint(ScaleMode, "ScaleMode");
	writequad(palinit16[0], "black", "palette");
	writequad(palinit16[1], "green", "palette");
	writequad(palinit16[2], "purple", "palette");
	writequad(palinit16[3], "white", "palette");
	writequad(palinit16[4], "black2", "palette");
	writequad(palinit16[5], "orange", "palette");
	writequad(palinit16[6], "blue", "palette");
	writequad(palinit16[7], "white2", "palette");
	// no duplicate key names are allowed in this app due to command line overrides
	writequad(defaultpalinit16[0],  "def_black", "default_palette");
	writequad(defaultpalinit16[1],  "def_green", "default_palette");
	writequad(defaultpalinit16[2],  "def_purple", "default_palette");
	writequad(defaultpalinit16[3],  "def_white", "default_palette");
	writequad(defaultpalinit16[4],  "def_black2", "default_palette");
	writequad(defaultpalinit16[5],  "def_orange", "default_palette");
	writequad(defaultpalinit16[6],  "def_blue", "default_palette");
	writequad(defaultpalinit16[7],  "def_white2", "default_palette");
}

void GetConsole() {
	static bool gotIt = false;	// only once per app

	if (gotIt) return;
	gotIt=true;

	if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
		// create a console for debugging
		AllocConsole();
	}

	int hCrt, i;
	FILE *hf;
	hCrt = _open_osfhandle((long) GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
	hf = _fdopen( hCrt, "w" );
	*stdout = *hf;
	i = setvbuf( stdout, NULL, _IONBF, 0 ); 
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

	// really shouldn't parse this ourselves, but it's late and I'm tired.
	// and now I build upon that evil! Muhaha! It's not even late this time!
	int tokenStart = -1;
	int idx=0;
	bool quote=false;
	if (m_lpCmdLine[0] != '\0') {
		strncpy(cmdLineCopy, m_lpCmdLine, sizeof(cmdLineCopy));
		cmdLineCopy[sizeof(cmdLineCopy)-1] = '\0';
		if (strlen(cmdLineCopy) < sizeof(cmdLineCopy)-2) {
			// pad with a space so we parse the last option
			strcat(cmdLineCopy, " ");
		}

		while (cmdLineCopy[idx] != '\0') {
			// handle escaped characters and quoted strings
			if (cmdLineCopy[idx] == '\\') {
				idx++;
				if (cmdLineCopy[idx] != '\0') idx++;
				continue;
			}
			if (cmdLineCopy[idx] == '\"') {
				quote=!quote;
				++idx;
				continue;
			}
			if ((cmdLineCopy[idx] == ' ') && (!quote)) {
				cmdLineCopy[idx] = '\0';
				if (tokenStart != -1) {
					// we had a string, what was it?
					if (cmdLineCopy[tokenStart] == '/') {
						// as an aside, check immediates here
						if (0 == strcmp("/verbose", &cmdLineCopy[tokenStart])) {
							GetConsole();	// open the console now
							fVerbose = true;
							debug("Verbose mode on");
						} else if (0 == strcmp("/?", &cmdLineCopy[tokenStart])) {
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
			if ((cmdLineCopy[idx]!='\0')&&(!isspace(cmdLineCopy[idx]))&&(tokenStart == -1)) {
				tokenStart = idx;
			}
			++idx;
		}
		// printf can handle NULL, so this is okay
		printf("File In: %s\nFileOut: %s\n", cmdFileIn, cmdFileOut);
	}

	// we might already have it depending on the command line
	GetConsole();

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
			debug("Skipping saving settings due to command line parameters.");
		}
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}


// pRGB - input image - 280x192x24-bit image
// p8Bit - output image - 280x192x8-bit image, we will provide palette
// pal - palette to use (8-color (really 6) Apple2e palette
void MYRGBTo8BitDithered(BYTE *pRGB, BYTE *p8Bit, MYRGBQUAD *inpal)
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
		if (g_OrderedDither) {
			quantize_new_ordered_percep(pRGB, p8Bit);
		} else {
			quantize_new_percep(pRGB, p8Bit);
		}
	} else {
		if (g_OrderedDither) {
			quantize_new_ordered(pRGB, p8Bit);
		} else {
			quantize_new(pRGB, p8Bit);
		}
	}
	AfxGetMainWnd()->EnableWindow(TRUE);
}

// create the two types of quantize_new, one for ordered dither, one for not
#undef QUANTIZE_ORDERED
#undef QUANTIZE_PERCEPTUAL
#undef quantize_common
#define quantize_common quantize_new
#include "quantize_common.h"

#define QUANTIZE_ORDERED
#undef QUANTIZE_PERCEPTUAL
#undef quantize_common
#define quantize_common quantize_new_ordered
#include "quantize_common.h"

#undef QUANTIZE_ORDERED
#define QUANTIZE_PERCEPTUAL
#undef quantize_common
#define quantize_common quantize_new_percep
#include "quantize_common.h"

#define QUANTIZE_ORDERED
#define QUANTIZE_PERCEPTUAL
#undef quantize_common
#define quantize_common quantize_new_ordered_percep
#include "quantize_common.h"

