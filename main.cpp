// Convert an image to a Apple2 HGR compatible image
// 

#define _WIN32_IE 0x0400

#include "stdafx.h"
#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <time.h>
#include <stdio.h>
#include <crtdbg.h>

#include "tipicview.h"
#include "D:\WORK\imgsource\4.0\islibs40_vs17_unicode\ISource.h"
#include "2passscale.h"
#include "TIPicViewDlg.h"

bool StretchHist;
void MYRGBTo8BitDithered(BYTE *pRGB, BYTE *p8Bit, MYRGBQUAD *pal, double dark);

#define MAXFILES 200000

int pixeloffset;
int heightoffset;
wchar_t szFiles[MAXFILES][256];
wchar_t *pTmp;
int iCnt, n, ret, errCount;
unsigned int idx1, idx2;
int ScaleMode;
int Background;
wchar_t szFolder[256];
wchar_t szBuf[256];
unsigned int iWidth, iHeight;
unsigned int inWidth, inHeight;
unsigned int outWidth, outHeight;
unsigned int finalW, finalH;
int currentx;
int currenty;
int currentw;
int currenth;
int maxerrorcount;
HISSRC hSource, hDest;
HGLOBAL hBuffer, hBuffer2;
unsigned char buf8[280*192];
RGBQUAD winpal[256];
bool fFirstLoad=false;
bool fRand;
bool fVerbose = false;
extern int g_orderSlide;
extern int g_nFilter;
extern int g_nPortraitMode;
extern int g_UsePerLinePalette;
extern int g_UseColorOnly;
extern int g_MatchColors;
extern double g_PercepR, g_PercepG, g_PercepB;
extern double g_LumaEmphasis;
extern double g_Gamma;
extern bool g_Grey;
extern int g_Perceptual;
extern int g_MaxColDiff;
extern int g_OrderedDither;
extern int g_MapSize;
extern wchar_t *cmdFileIn;		// from command line, makes a non-GUI mode
extern wchar_t *cmdFileOut;
extern HGLOBAL load_gif(wchar_t *filename, unsigned int *iWidth, unsigned int *iHeight);
MYRGBQUAD pal[256];

extern LPVOID pSharedPointer;
extern wchar_t szLastFilename[256];	// used to make sure we don't reload our own shared file

int instr(unsigned short *, char*);
bool ScalePic(int nFilter, int nPortraitMode);
void BuildFileList(wchar_t *szFolder);
BOOL ResizeRGBBiCubic(BYTE *pImgSrc, UINT32 uSrcWidthPix, UINT32 uSrcHeight, BYTE *pImgDest, UINT32 uDestWidthPix, UINT32 uDestHeight);

MYRGBQUAD palinit16[256] = {
	// palette from http://mrob.com/pub/xapple2/colors.html
	0,0,0,0,		// black
	20,245,60,0,	// green
	255,68,253,0,	// purple
	255,255,255,0,	// white

	0,0,0,0,		// black2
	255,106,60,0,	// orange
	20,207,253,0,	// blue
	255,255,255,0	// white
};

// remembers the default palette for the settings file
MYRGBQUAD defaultpalinit16[16];

// ordered dither threshold maps
// 3 and 8 didn't look very good due to color clash
double g_thresholdMap2x2[2][2] = {
	0, 2,
	3, 1
};
double g_thresholdMap4x4[4][4] = {
	 0,	 8,	 2,	10,
	12,	 4,	14,	 6,
	 3,	11,	 1,	 9,
	15,	 7,	13,	 5
};

unsigned char orig8[280*192];
CWnd *pWnd;

// simple wrapper for debug - command line mode is normally quiet unless verbose is set
void debug(wchar_t *s, ...) {
	if ((cmdFileIn == NULL) || (fVerbose)) {
		wchar_t buf[1024];

		_vsnwprintf(buf, 1023, s, (char*)((&s)+1));
		buf[1023]='\0';

		OutputDebugString(buf);
		printf("%S",buf);
	}
}

// handles invalid arguments to strcpy_s and strcat_s, etc
void app_handler(const wchar_t * expression,
                 const wchar_t * function, 
                 const wchar_t * file, 
                 unsigned int line,
                 uintptr_t pReserved) {
   printf("Warning: path or string too long. Skipping.\n");
}

// Mode 0 - pass in a path, random image from that path
// Mode 1 - reload image - if pFile is NULL, use last from list, else use passed file
// Mode 2 - load specific image and remember it
// 'dark' is a value from 0-16 and is the amount to darken for ordered dither
void maincode(int mode, CString pFile, double dark)
{
	// initialize
	bool fArgsOK=true;
	static bool fHaveFiles=false;
	static bool initialized=false;
	wchar_t szFileName[256];
	static int nlastpick=-1;
	int noldlast;
	wchar_t szOldFN[256];

	if (!initialized) {
		IS40_Initialize("{887916EA-FAE3-12E2-19C1-8B0FC40F045F}");	// please do not reuse this key in other applications
		srand((unsigned)time(NULL));
		initialized=true;
		pWnd=AfxGetMainWnd();

		// I build this on the assumption that the _s string functions would just
		// truncate and move on on long strings. that's untrue - they assert and fatal.
		// We can override that for now like this:
		_CrtSetReportMode(_CRT_ASSERT, 0);
		_set_invalid_parameter_handler(app_handler);
		
		// TODO: I really have paths this long. I should fix this.

		// update threshold maps - they're supposed to be fractional
		for (int i1=0; i1<2; i1++) {
			for (int i2=0; i2<2; i2++) {
				g_thresholdMap2x2[i1][i2] /= (double)(4.0);
			}
		}
		for (int i1=0; i1<4; i1++) {
			for (int i2=0; i2<4; i2++) {
				g_thresholdMap4x4[i1][i2] /= (double)(16.0);
			}
		}
	}

	if (mode == 0) {
		// Set defaults
		wcscpy_s(szFolder, 256, pFile);
	}

	iWidth=280;
	iHeight=192;

	Background=0;
	fRand=false;
	
	maxerrorcount=6;

	if (mode == 0) {
		if ((!fHaveFiles)&&(pFile.GetLength() > 0)) {
			// get list of files
			iCnt=0;
			BuildFileList(szFolder);

			printf("\n%d files found.\n", iCnt);

			if (0==iCnt) {
				printf("That's an error\n");
				AfxMessageBox(_T("The 'rnd' function currently only works if you have a c:\\pics folder."));
				return;
			}

			fHaveFiles=true;
		}
	}

	// Used to fit the image into the remaining space
	currentx=0;
	currenty=0;
	currentw=iWidth;
	currenth=iHeight;
	hBuffer2=NULL;
	noldlast=nlastpick;
	if (noldlast != -1) {
		wcscpy_s(szOldFN, 256, szFiles[nlastpick]);
	}


	{
		int cntdown;

		errCount=0;
		ScaleMode=-1;

		// randomly choose one
	tryagain:
		hBuffer = NULL;

		if ((1 == mode) || (2 == mode)) {
			if ((pFile.IsEmpty()) && (-1 != nlastpick)) {
				n=nlastpick;
			} else {
				if (pFile.IsEmpty()) {
					return;
				}
				n=MAXFILES-1;
				wcscpy_s(szFiles[n], 256, pFile);
			}
		} else {
			if (nlastpick!=-1) {
				wcscpy_s(szFiles[nlastpick],256, _T(""));		// so we don't choose it again
			}
			pixeloffset=0;
			heightoffset=0;
			errCount++;
			if (errCount>6) {
				printf("Too many errors - stopping.\n");
				if (noldlast != -1) {
					wcscpy_s(szFiles[noldlast], 256, szOldFN);
					nlastpick=noldlast;
				}
				return;
			}

			n=((rand()<<16)|rand())%iCnt;
			cntdown=iCnt;
			while (wcslen(szFiles[n]) == 0) {
				n++;
				if (n>=iCnt) n=0;
				cntdown--;
				if (cntdown == 0) {
					printf("Ran out of images in the list, aborting.\n");
					if (noldlast != -1) {
						wcscpy_s(szFiles[noldlast], 256, szOldFN);
						nlastpick=noldlast;
					}
					return;
				}
			}
		}

		wprintf(L"Chose #%d: %s\n", n, &szFiles[n][0]);
		wcscpy_s(szFileName, 256, szFiles[n]);
		nlastpick=n;

		if (NULL != hBuffer) {
			goto ohJustSkipTheLoad;
		}

		// dump the data into the shared memory, if available (and not from there)
		wcsncpy(szLastFilename, szFileName, 255);		// save the name locally first so we can compare
		if ((pSharedPointer)&&((mode == 0)||(mode==2))) {
			// we copy the first byte last to ensure it's atomic
			memcpy((char*)pSharedPointer+sizeof(szFileName[0]), szFileName+1, sizeof(szFileName));
			InterlockedExchange((volatile LONG*)pSharedPointer, *((LONG*)szFileName));	// actually writes 32-bits, but guaranteed execution across cores
		}

		// open the file
		hSource=IS40_OpenFileSource(szFileName);
		if (NULL==hSource) {
			printf("Can't open image file.\n");
			return;
		}

		// guess filetype
		ret=IS40_GuessFileType(hSource);
		IS40_Seek(hSource, 0, 0);

		switch (ret)
		{
		case 1: //bmp
			hBuffer=IS40_ReadBMP(hSource, &inWidth, &inHeight, 24, NULL, 0);
			break;

		case 2: //gif
			IS40_CloseSource(hSource);
			hSource=NULL;
			hBuffer=load_gif(szFileName, &inWidth, &inHeight);
			break;

		case 3: //jpg
			hBuffer=IS40_ReadJPG(hSource, &inWidth, &inHeight, 24, 0);
			break;

		case 4: //png
			hBuffer=IS40_ReadPNG(hSource, &inWidth, &inHeight, 24, NULL, 0);
			break;

		case 5: //pcx
			hBuffer=IS40_ReadPCX(hSource, &inWidth, &inHeight, 24, NULL, 0);
			break;

		case 6: //tif
			hBuffer=IS40_ReadTIFF(hSource, &inWidth, &inHeight, 24, NULL, 0, 0);
			break;

		default:
			// not something supported, then
			printf("Unable to identify file (corrupt?)\n-> %S <-\n", szFileName);
			IS40_CloseSource(hSource);
			return;
		}

		if (NULL != hSource) {
			IS40_CloseSource(hSource);
		}

		if (NULL == hBuffer) {
			printf("Failed reading image file. (%d)\n", IS40_GetLastError());
			GlobalFree(hBuffer);
			if (mode == 1) goto tryagain; else return;
		}

ohJustSkipTheLoad:

		// if needed, convert to greyscale (using the default formula)
		if (g_Grey) {
			unsigned char *pBuf = (unsigned char*)hBuffer;
			for (unsigned int idx=0; idx<inWidth*inHeight*3; idx+=3) {
				int r,g,b,x;
				r=pBuf[idx];
				g=pBuf[idx+1];
				b=pBuf[idx+2];
				if (g_Perceptual) {
					x=(int)(((double)r*g_PercepR) + ((double)g*g_PercepG) + ((double)b*g_PercepB) + 0.5);
				} else {
					double y;
					makeY(r, g, b, y);
					x=(int)y;
				}
				if (x>255) x=255;
				if (x<0) x=0;
				pBuf[idx]=x;
				pBuf[idx+1]=x;
				pBuf[idx+2]=x;
			}			
		}

		// scale the image
		memcpy(pal, palinit16, sizeof(pal));
		for (int idx=0; idx<256; idx++) {
			// reorder for the windows draw
			winpal[idx].rgbBlue=pal[idx].rgbBlue;
			winpal[idx].rgbRed=pal[idx].rgbRed;
			winpal[idx].rgbGreen=pal[idx].rgbGreen;
		}

		// cache these values from ScalePic for the filename code
		int origx, origy, origw, origh;
		// no, not very clean code ;) And I started out so well ;)
		origx=currentx;
		origy=currenty;
		origw=currentw;
		origh=currenth;
		ScaleMode=-1;

		if (!ScalePic(g_nFilter, g_nPortraitMode)) {			// from hBuffer to hBuffer2
			// failed due to scale 
			if (mode == 1) goto tryagain; else return;
		}

		GlobalFree(hBuffer);

		// flag that we actually used the current setting
		((CTIPicViewDlg*)pWnd)->MakeConversionModeValid();
	}

	{
		// original before adjustments
		if (StretchHist) {
			debug(_T("Equalize histogram...\n"));
			if (!IS40_BrightnessHistogramEqualizeImage((unsigned char*)hBuffer2, iWidth, iHeight, 3, iWidth*3, 32, 224, 0)) {
				printf("Failed to equalize image, code %d\n", IS40_GetLastError());
			}
		}

		// apply gamma
		if ((g_Gamma != 1.0)&&(g_Gamma != 0.0)) {
			// http://www.dfstudios.co.uk/articles/programming/image-programming-algorithms/image-processing-algorithms-part-6-gamma-correction/
			double correct = 1/g_Gamma;
			unsigned char *pWork = (unsigned char*)hBuffer2;
			for (int idx=0; idx<280*192*3; idx++) {
				// every pixel is handled the same, so we just walk through it all at once
				int pix = *(pWork+idx);
				double newpix = pow(((double)pix/255), correct);
				*(pWork+idx) = (unsigned char)(newpix*255);
			}
		}

		// color shift if not using palette
		{
			if (g_MaxColDiff > 0) {
				// diff is 0-100% for how far we can shift towards a valid color
				// max distance in color space is SQRT(255^2+255^2+255^2) = 441.673 units (so the 255 is kind of arbitrary)
				const double maxrange = ((255*255)+(255*255)+(255*255))*(((double)g_MaxColDiff/100.0)*((double)g_MaxColDiff/100.0));		// scaled distance squared
				unsigned char *pWork = (unsigned char*)hBuffer2;
				for (int idx=0; idx<280*192; idx++) {
					int r = *(pWork);
					int g = *(pWork+1);
					int b = *(pWork+2);
					double mindist = ((255*255)+(255*255)+(255*255));
					int best = 0;
					for (int i2=0; i2<8; i2++) {
						int rd = r-pal[i2].rgbRed;
						int gd = g-pal[i2].rgbGreen;
						int bd = b-pal[i2].rgbBlue;
						double dist = (rd*rd)+(gd*gd)+(bd*bd);
						if (dist < mindist) {
							mindist = dist;
							best = i2;
						}
					}
					// we have a distance and a best color
					if (mindist <= maxrange) {
						// it's in range, just directly map it
						r = pal[best].rgbRed;
						g = pal[best].rgbGreen;
						b = pal[best].rgbBlue;
					} else {
						// not in range, so calculate a scale to move by
						double scale = maxrange / mindist;	// x% ^2
						scale = sqrt((double)scale);				// x% raw
						double move = (pal[best].rgbRed - r) * scale;
						if (move < 0) {
							r += (int)(move - 0.5);
						} else {
							r += (int)(move + 0.5);
						}
						move = (pal[best].rgbGreen - g) * scale;
						if (move < 0) {
							g += (int)(move - 0.5);
						} else {
							g += (int)(move + 0.5);
						}
						move = (pal[best].rgbBlue - b) * scale;
						if (move < 0) {
							b += (int)(move - 0.5);
						} else {
							b += (int)(move + 0.5);
						}
					}
					*(pWork) = r;
					*(pWork+1) = g;
					*(pWork+2) = b;
					pWork+=3;
				}
			}
		}

		// original after adjustments
		{
			if (NULL != pWnd) {
				if (cmdFileIn == NULL) {
					printf("Drawing source image...\n");
				}
				pWnd->SetWindowText(szFileName);
				CDC *pCDC=pWnd->GetDC();
				if (NULL != pCDC) {
					int dpi = GetDpiForWindow(pWnd->GetSafeHwnd());
					if (!IS40_StretchDrawRGB(*pCDC, (unsigned char*)hBuffer2, iWidth, iHeight, iWidth*3, DPIFIX(XOFFSET), DPIFIX(0), DPIFIX(iWidth*2), DPIFIX(iHeight*2))) {
						printf("ISDrawRGB failed, code %d\n", IS40_GetLastError());
					}
					pWnd->ReleaseDC(pCDC);
				}
			}
		}

		debug(_T("Reducing colors...\n"));
		MYRGBTo8BitDithered((unsigned char*)hBuffer2, buf8, pal, dark);
	}

	debug(_T("\n"));

	// Draw the image 
	if (NULL != pWnd) {
		debug(_T("Draw final image...\n"));
		CDC *pCDC=pWnd->GetDC();
		if (NULL != pCDC) {
				int dpi = GetDpiForWindow(pWnd->GetSafeHwnd());
				if (!IS40_StretchDraw8Bit(*pCDC, buf8, iWidth, iHeight, iWidth, winpal, DPIFIX(XOFFSET), DPIFIX(0), DPIFIX(iWidth*2), DPIFIX(iHeight*2))) {
				printf("ISDraw8 failed, code %d\n", IS40_GetLastError());
			}
			pWnd->ReleaseDC(pCDC);
		}
		// and while we've got it, if there's an output name
		if (cmdFileOut != NULL) {
			CString csName = cmdFileOut;
			csName+=".BMP";
			HISDEST dst = IS40_OpenFileDest(csName, FALSE);
			if (NULL != dst) {
				IS40_WriteBMP(dst, buf8, iWidth, iHeight, 8, iWidth, 256, winpal, NULL, 0);
				IS40_CloseDest(dst);
			}
		}
	}

	fFirstLoad=true;
	delete[] hBuffer2;
}

void BuildFileList(wchar_t *szFolder)
{
	HANDLE hIndex;
	WIN32_FIND_DATA dat;
	wchar_t buffer[256];

	wcscpy_s(buffer, 256, szFolder);
	wcscat_s(buffer, 256, _T("\\*.*"));
	hIndex=FindFirstFile(buffer, &dat);

	while (INVALID_HANDLE_VALUE != hIndex) {
		if (iCnt>MAXFILES-1) {
			FindClose(hIndex);
			return;
		}
		
		wcscpy_s(buffer, 256, szFolder);
		wcscat_s(buffer, 256, _T("\\"));
		wcscat_s(buffer, 256, dat.cFileName);

		if (dat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (dat.cFileName[0]=='.') goto next;
			BuildFileList(buffer);
			goto next;
		}

		// BMP, GIF, JPG, PNG, PCX, TIF
		// Check last few characters
		if ((0==_wcsicmp(&buffer[wcslen(buffer)-3], _T("bmp"))) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-4], _T("tiap"))) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-3], _T("gif"))) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-3], _T("jpg"))) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-4], _T("jpeg"))) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-3], _T("jpc"))) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-3], _T("png"))) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-3], _T("pcx"))) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-4], _T("tiff"))) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-3], _T("tif")))) {
				wcscpy_s(&szFiles[iCnt++][0], 256, buffer);
		}

next:
		if (false == FindNextFile(hIndex, &dat)) {
			int ret;
			if ((ret=GetLastError()) != ERROR_NO_MORE_FILES) {
				OutputDebugString(_T("Error in findnextfile\n"));
			}
			FindClose(hIndex);
			hIndex=INVALID_HANDLE_VALUE;
		}
	}
}

// Scales from hBuffer to hBuffer2
bool ScalePic(int nFilter, int nPortraitMode)
{
#define X_AXIS 1
#define Y_AXIS 2
	
	double x1,y1,x_scale,y_scale;
	int r;
	unsigned int thisx, thisy;
	HGLOBAL tmpBuffer;
	
	debug(_T("Image:  %d x %d\n"),inWidth, inHeight);
	debug(_T("Output: %d x %d\n"),currentw, currenth);
	
	x1=(double)(inWidth);
	y1=(double)(inHeight);
	
	// scale needs to be adjusted. Input pixels are 1:1, but Apple pixels are not square,
	// they are 0.9142857 times as wide, and we double them for the half-pixel shift (256/280)
	x_scale=((double)(currentw))/x1;
	y_scale=((double)(currenth))/y1;

	debug(_T("Scale:  %f x %f\n"),x_scale,y_scale);


	if (ScaleMode == -1) {
		ScaleMode=Y_AXIS;
	
		if (y1*(x_scale*(256.0/280.0)) > (double)(currenth)) ScaleMode=Y_AXIS;
		if (x1*(y_scale/(256.0/280.0)) > (double)(currentw)) ScaleMode=X_AXIS;
		debug(_T("Decided scale (1=X, 2=Y): %d\n"),ScaleMode);
	} else {
		debug(_T("Using scale (1=X, 2=Y): %d\n"),ScaleMode);
	}
	
	// "portrait mode" is used for both X and Y axis as a fill mode now
	switch (nPortraitMode) {
		default:
		case 0:		// full image
			// no change
			break;

		case 1:		// top/left
		case 2:		// middle
		case 3:		// bottom/right
			// use other axis instead, and we'll crop it after we scale
			debug(_T("Scaling for fill mode (opposite scale used).\n"));
			if (ScaleMode == Y_AXIS) {
				ScaleMode=X_AXIS;
			} else {
				ScaleMode=Y_AXIS;
			}
			break;
	}

	if (ScaleMode==Y_AXIS) {
		x1*=y_scale/(256.0/280.0);	
		y1*=y_scale;
	} else {
		x1*=x_scale;
		y1*=x_scale*(256.0/280.0);
	}
	
	x1+=0.5;
	y1+=0.5;
	finalW=(int)(x1);
	finalH=(int)(y1);

	debug(_T("Output size: %d x %d\n"), finalW, finalH);

	switch (nFilter) {
		case 0 :
			{
				C2PassScale <CBoxFilter> ScaleEngine;
				tmpBuffer=ScaleEngine.AllocAndScale((unsigned char*)hBuffer, inWidth, inHeight, finalW, finalH);
			}
			break;

		case 1 :
			{
				C2PassScale <CGaussianFilter> ScaleEngine;
				tmpBuffer=ScaleEngine.AllocAndScale((unsigned char*)hBuffer, inWidth, inHeight, finalW, finalH);
			}
			break;

		case 2 :
			{
				C2PassScale <CHammingFilter> ScaleEngine;
				tmpBuffer=ScaleEngine.AllocAndScale((unsigned char*)hBuffer, inWidth, inHeight, finalW, finalH);
			}
			break;

		case 3 :
			{
				C2PassScale <CBlackmanFilter> ScaleEngine;
				tmpBuffer=ScaleEngine.AllocAndScale((unsigned char*)hBuffer, inWidth, inHeight, finalW, finalH);
			}
			break;

		default:
		case 4 :
			{
				C2PassScale <CBilinearFilter> ScaleEngine;
				tmpBuffer=ScaleEngine.AllocAndScale((unsigned char*)hBuffer, inWidth, inHeight, finalW, finalH);
			}
			break;
	}

	// apply portrait cropping if needed, max Y is 192 pixels
	if (finalH > 192) {
		int y=0;

		debug(_T("Cropping...\n"));

		switch (nPortraitMode) {
			default:
			case 0:		// full
				debug(_T("This could be a problem - image scaled too large!\n"));
				// no change
				break;

			case 1:		// top
				// just tweak the size
				break;

			case 2:		// middle
				// move the middle to the top
				y=(finalH-192)/2;
				break;

			case 3:		// bottom
				// move the bottom to the top
				y=(finalH-192);
				break;
		}
		if (heightoffset != 0) {
			debug(_T("Vertical nudge %d pixels...\n"), heightoffset);

			y+=heightoffset;
			while (y<0) { 
				y++;
				heightoffset++;
			}
			while ((unsigned)y+191 >= finalH) {
				y--;
				heightoffset--;
			}
		}
		finalH=192;
		if (y > 0) {
			memmove(tmpBuffer, (unsigned char*)tmpBuffer+y*finalW*3, 192*finalW*3);
		}
	}
	// apply landscape cropping if needed, max X is 280 pixels
	if (finalW > 280) {
		int x;
		unsigned char *p1,*p2;

		debug(_T("Cropping...\n"));

		switch (nPortraitMode) {
			default:
			case 0:		// full
				debug(_T("This could be a problem - image scaled too large!\n"));
				// no change
				x=-1;
				break;

			case 1:		// top
				// just copy the beginning rows
				x=0;
				break;

			case 2:		// middle
				// move the middle 
				x=(finalW-280)/2;
				break;

			case 3:		// bottom
				// move the right edge
				x=(finalW-280);
				break;
		}
		if (x >= 0) {
			// we need to move each row manually!
			p1=(unsigned char*)tmpBuffer;
			p2=p1+x*3;
			for (unsigned int y=0; y<finalH; y++) {
				memmove(p1, p2, 280*3);
				p2+=finalW*3;
				p1+=280*3;
			}
		}
		finalW=280;
	}

	if (NULL == hBuffer2) {
		hBuffer2=new BYTE[iWidth * iHeight * 3];
		ZeroMemory(hBuffer2, iWidth * iHeight * 3);
	}

	// calculate the exact position. If this is the last image (remaining space will be too small),
	// then we will center this one in the remaining space
	thisx=(currentw-finalW)/2;
	thisy=(currenth-finalH)/2;

	currenth-=finalH;
	currentw-=finalW;
	currentx+=finalW;
	currenty+=finalH;

	r=IS40_OverlayImage((unsigned char*)hBuffer2, iWidth, iHeight, 3, iWidth*3, (unsigned char*)tmpBuffer, finalW, finalH, finalW*3, thisx, thisy, 1.0, 0, 0, 0);
	if (false==r) {
		printf("Overlay failed.\n");
		return false;
	}
	delete tmpBuffer;

	// handle shifting
	if (pixeloffset != 0) {
		debug(_T("Nudging %d pixels...\n"), pixeloffset);
		if (pixeloffset > 0) {
			memmove(hBuffer2, (char*)hBuffer2+pixeloffset*3, (280*192-pixeloffset)*3);
		} else {
			memmove((char*)hBuffer2-pixeloffset*3, hBuffer2, (280*192+pixeloffset)*3);
		}
	}

	return true;
}

int instr(unsigned short *s1, char *s2)
{
	while (*s1)
	{
		if (*s1 != *s2) {
			s1++;
		} else {
			break;
		}
	}

	if (0 == *s1) {
		return 0;
	}

	while (*s2)
	{
		if (*(s1++) != *(s2++)) {
			return 0;
		}
	}

	return 1;
}


