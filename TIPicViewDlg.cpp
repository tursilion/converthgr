// TIPicViewDlg.cpp : implementation file
//

#include "stdafx.h"
#include <Windows.h>
#include "TIPicView.h"
#include "TIPicViewDlg.h"
#include "D:\WORK\imgsource\4.0\islibs40_vs17_unicode\ISource.h"
#include "DOS3.3.cpp"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Floppy diskette tools
const unsigned char hello[] = {
    0x1f,0,7,8,10,0,0x91,0,0x1d,8,0x14,0,0xba,0x22,4,0x42,
    0x4c,0x4f,0x41,0x44,0x20,0x48,0x47,0x52,0x46,0x49,0x4c,
    0x45,0x22,0,0,0,0x8c,0x45
};

extern bool StretchHist;
static bool fInSlideMode=false;
extern int PIXA,PIXB,PIXC,PIXD,PIXE,PIXF;
extern int g_orderSlide;
extern int g_nFilter;
extern int g_nPortraitMode;
extern int pixeloffset, heightoffset;
extern int g_Perceptual;
extern int g_AccumulateErrors;
extern int g_MaxColDiff;
extern int g_OrderedDither;
extern int g_MapSize;
extern double g_PercepR, g_PercepG, g_PercepB;
extern double g_LumaEmphasis;
extern double g_Gamma;
extern wchar_t *cmdFileIn, *cmdFileOut;
extern bool g_Grey;
unsigned char pbuf[8192];
extern unsigned char buf8[280*192];
extern bool fFirstLoad;
extern RGBQUAD winpal[256];
extern bool fVerbose;

int nLastValidConversionMode = 0;
// handle to global shared memory used for multiple instances to share random fileloads
// this just makes it easier to compare different render options side by side, hopefully
// it won't break anything
HANDLE hSharedMemory = NULL;
LPVOID pSharedPointer = NULL;
wchar_t szLastFilename[256];

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTIPicViewDlg dialog

CTIPicViewDlg::CTIPicViewDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CTIPicViewDlg::IDD, pParent)
	, m_nOrderSlide(0)
{
	//{{AFX_DATA_INIT(CTIPicViewDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	StretchHist=false;
	PIXA=1;
	PIXB=2;
	PIXC=2;
	PIXD=2;
	PIXE=1;
	PIXF=1;
}

void CTIPicViewDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTIPicViewDlg)
	// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
	DDX_Text(pDX, IDC_EDIT1, m_pixelD);
	DDV_MinMaxUInt(pDX, m_pixelD, 0, 16);
	DDX_Text(pDX, IDC_EDIT2, m_pixelC);
	DDV_MinMaxUInt(pDX, m_pixelC, 0, 16);
	DDX_Text(pDX, IDC_EDIT3, m_pixelB);
	DDV_MinMaxUInt(pDX, m_pixelB, 0, 16);
	DDX_Text(pDX, IDC_EDIT4, m_pixelA);
	DDV_MinMaxUInt(pDX, m_pixelA, 0, 16);
	DDX_Text(pDX, IDC_EDIT6, m_pixelE);
	DDV_MinMaxUInt(pDX, m_pixelE, 0, 16);
	DDX_Text(pDX, IDC_EDIT5, m_pixelF);
	DDV_MinMaxUInt(pDX, m_pixelF, 0, 16);
	DDX_CBIndex(pDX, IDC_FILTER, m_nFilter);
	DDX_CBIndex(pDX, IDC_PORTRAIT, m_nPortraitMode);
	DDX_CBIndex(pDX, IDC_ERRMODE, m_ErrMode);
	DDX_Text(pDX, IDC_COLDIFF, m_MaxColDiff);
	DDV_MinMaxInt(pDX, m_MaxColDiff, 0, 100);
	DDX_Text(pDX, IDC_PERPR, m_PercepR);
	DDV_MinMaxInt(pDX, m_PercepR, 0, 100);
	DDX_Text(pDX, IDC_PERPG, m_PercepG);
	DDV_MinMaxInt(pDX, m_PercepG, 0, 100);
	DDX_Text(pDX, IDC_PERPB, m_PercepB);
	DDV_MinMaxInt(pDX, m_PercepB, 0, 100);
	DDX_Control(pDX, IDC_GREY, m_CtrlGrey);
	DDX_Text(pDX, IDC_LUMA, m_Luma);
	DDX_Text(pDX, IDC_GAMMA, m_Gamma);
	DDX_Slider(pDX, IDC_ORDERSLIDE, m_nOrderSlide);
	DDV_MinMaxInt(pDX, m_nOrderSlide, 0, 16);
	DDX_Control(pDX, IDC_ORDERSLIDE, ctrlOrderSlide);
}

BEGIN_MESSAGE_MAP(CTIPicViewDlg, CDialog)
	//{{AFX_MSG_MAP(CTIPicViewDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON1, OnButton1)
	ON_BN_CLICKED(IDC_BUTTON2, OnButton2)
	ON_BN_CLICKED(IDC_CHECK1, OnCheck1)
	ON_BN_CLICKED(IDC_RND, OnRnd)
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_BUTTON4, OnButton4)
	ON_BN_DOUBLECLICKED(IDC_RND, OnDoubleclickedRnd)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_DROPFILES()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_BUTTON3, &CTIPicViewDlg::OnBnClickedButton3)
	ON_BN_CLICKED(IDC_BUTTON5, &CTIPicViewDlg::OnBnClickedButton5)
	ON_BN_CLICKED(IDC_PERCEPT, &CTIPicViewDlg::OnBnClickedPercept)
	ON_BN_CLICKED(IDC_RELOAD, &CTIPicViewDlg::OnBnClickedReload)
	ON_BN_CLICKED(IDC_DISTDEFAULT, &CTIPicViewDlg::OnBnClickedFloyd)
	ON_BN_CLICKED(IDC_PERPRESET, &CTIPicViewDlg::OnBnClickedPerpreset)
	ON_BN_CLICKED(IDC_PATTERN, &CTIPicViewDlg::OnBnClickedPattern)
	ON_BN_CLICKED(IDC_NODITHER, &CTIPicViewDlg::OnBnClickedNodither)
	ON_BN_CLICKED(IDC_MONO, &CTIPicViewDlg::OnBnClickedMono)
	ON_BN_CLICKED(IDC_GREEN, &CTIPicViewDlg::OnBnClickedGreen)
	ON_BN_CLICKED(IDC_DISTDEFAULT2, &CTIPicViewDlg::OnBnClickedAtkinson)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_ORDERED, &CTIPicViewDlg::OnBnClickedOrdered)
	ON_BN_CLICKED(IDC_ORDERED2, &CTIPicViewDlg::OnBnClickedOrdered2)
	ON_BN_CLICKED(IDC_DIAG, &CTIPicViewDlg::OnBnClickedDiag)
	ON_BN_CLICKED(IDC_ORDERED3, &CTIPicViewDlg::OnBnClickedOrdered3)
	ON_BN_CLICKED(IDC_ORDERED4, &CTIPicViewDlg::OnBnClickedOrdered4)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTIPicViewDlg message handlers

BOOL CTIPicViewDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// set up the slider
	ctrlOrderSlide.SetRange(0, 16);

	// update configuration pane
	m_pixelA=PIXA;
	m_pixelB=PIXB;
	m_pixelC=PIXC;
	m_pixelD=PIXD;
	m_pixelE=PIXE;
	m_pixelF=PIXF;
	m_nFilter=g_nFilter;
	m_nPortraitMode=g_nPortraitMode;
	m_ErrMode=g_AccumulateErrors?1:0;
	m_MaxColDiff = g_MaxColDiff;
	m_PercepR = (int)(g_PercepR*100.0);
	m_PercepG = (int)(g_PercepG*100.0);
	m_PercepB = (int)(g_PercepB*100.0);
	m_Luma = (int)(g_LumaEmphasis*100.0);
	m_Gamma = (int)(g_Gamma*100.0);
	PrepareData();
	
	// interface to the dithering ordered buttons
	if (g_OrderedDither != 0) {
		EnableDitherCtrls(false);
		int option = g_OrderedDither + (g_MapSize==4 ? 2: 0);
		switch (option) {
		case 1: SendDlgItemMessage(IDC_ORDERED, BM_SETCHECK, BST_CHECKED, 0); break;
		case 2: SendDlgItemMessage(IDC_ORDERED2, BM_SETCHECK, BST_CHECKED, 0); break;
		case 3: SendDlgItemMessage(IDC_ORDERED3, BM_SETCHECK, BST_CHECKED, 0); break;
		case 4: SendDlgItemMessage(IDC_ORDERED4, BM_SETCHECK, BST_CHECKED, 0); break;
		}
	}

	// hacky command line interface
	if (cmdFileIn != NULL) {
		LaunchMain(2, cmdFileIn);	// load
	}
	if (cmdFileOut != NULL) {
		OnButton4();		// save
		EndDialog(IDOK);	// and quit
	}

	InterlockedExchange(&cs, 0);		// no thread running yet
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CTIPicViewDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CTIPicViewDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

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
		CPaintDC dc(this);

		if (NULL != buf8) {
			int dpi = GetDpiForWindow(GetSafeHwnd());
			IS40_StretchDraw8Bit(dc, buf8, 280, 192, 280, winpal, DPIFIX(XOFFSET), DPIFIX(0), DPIFIX(280*2), DPIFIX(192*2));
		}
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CTIPicViewDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CTIPicViewDlg::OnOK() {}

void maincode(int mode, CString pFile, double dark);
// this is actually 'Open' now
void CTIPicViewDlg::OnRnd() 
{
	CString csPath;

	if (fInSlideMode) {
		OnDoubleclickedRnd();
		return;
	}

	// Get Path filename
	CFileDialog dlg(true);
	dlg.m_ofn.lpstrTitle=_T("Select file to open: BMP, GIF, JPG, PNG, PCX or TIFF");
	if (IDOK != dlg.DoModal()) {
		return;
	}
	csPath=dlg.GetPathName();

	LaunchMain(2, (char*)csPath.GetString());

}

void CTIPicViewDlg::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	fInSlideMode=true;
	CWnd *pCtl=GetDlgItem(IDC_RND);
	if (pCtl) pCtl->SetWindowText(_T("Next"));
	SetTimer(0, 500, NULL);		// start a regular tick to watch for new files

	CDialog::OnLButtonDblClk(nFlags, point);
}

// This function doesn't get called normally, but that's okay, we can call it the long way ;)
void CTIPicViewDlg::OnDoubleclickedRnd() 
{
	static CString csPath;

	if (csPath=="") {
		// Get Path image
		CFileDialog dlg(true);
		dlg.m_ofn.lpstrTitle=_T("Select any file in a folder for random load - filename will be ignored");
		if (IDOK != dlg.DoModal()) {
			return;
		}
		csPath=dlg.GetPathName();
		if (csPath.ReverseFind('\\') != -1) {
			csPath=csPath.Left(csPath.ReverseFind('\\'));
		}
	}

	// make sure the shared memory is available
	if (NULL == hSharedMemory) {
		hSharedMemory = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, _T("Convert9918MappedData"));
		if (NULL != hSharedMemory) {
			pSharedPointer = MapViewOfFile(hSharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, 4096);
		} else {
			pSharedPointer = NULL;
		}
	}

	LaunchMain(0,csPath);
	// display the picture on the dialog
}


void CTIPicViewDlg::OnCancel() 
{
}

void CTIPicViewDlg::OnButton2() 
{
	if (pixeloffset < 7) {
		pixeloffset++;
		LaunchMain(1,"");
	}
}

void CTIPicViewDlg::OnButton1() 
{
	if (pixeloffset > -7) {
		pixeloffset--;
		LaunchMain(1,"");
	}
}

// stretch histogram
void CTIPicViewDlg::OnCheck1() 
{
	CButton *pCtrl=(CButton*)GetDlgItem(IDC_CHECK1);
	if (pCtrl) {
		if (pCtrl->GetCheck() == BST_CHECKED) {
			StretchHist=true;
		} else {
			StretchHist=false;
		}
	}
}

void CTIPicViewDlg::OnClose() 
{
	EndDialog(IDOK);
	CDialog::OnClose();
	if (pSharedPointer) {
		UnmapViewOfFile(pSharedPointer);
		pSharedPointer = NULL;
	}
	if (hSharedMemory) {
		CloseHandle(hSharedMemory);
	}
}

void CTIPicViewDlg::OnButton4() 
{
	FILE *fP = NULL;

	memset(pbuf, 0, sizeof(pbuf));

	if (!fFirstLoad) {
		if (fVerbose) {
			debug(_T("No file loaded to save.\n"));
		}
		return;
	}

	// Type:		 1							   2						    3
	CString csFmt = _T("DOS 3.3 Style DSK image|*.DSK|Apple2 HGR Binary File|*.HGR|PNG file (PC)|*.PNG||");
	CFileDialog dlg(false, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, csFmt);

	// Save image
	dlg.m_ofn.lpstrTitle=_T("Enter filename to save as....");

	if (!ResetToValidConversion()) {
		return;
	}

	if ((cmdFileOut!=NULL) || (IDOK == dlg.DoModal())) {
		CString cs, outfile, rawfile;
		bool bOverwrite=false;

		outfile=dlg.GetPathName();
		if (cmdFileOut != NULL) {
			outfile = cmdFileOut;
		}

		// strip extension if present
		if (outfile.ReverseFind('.') != -1) {
			outfile=outfile.Left(outfile.ReverseFind('.'));
		}
		
		switch (dlg.m_pOFN->nFilterIndex) {
		case 1: // DSK
			if (outfile.Right(4).CompareNoCase(_T(".dsk")) != 0) outfile+=_T(".DSK");
			break;
		default:
		case 2:	// HGR
			if (outfile.Right(4).CompareNoCase(_T(".hgr")) != 0) outfile+=_T(".hgr");
			break;
		case 3:	// PC PNG
			if (outfile.Right(4).CompareNoCase(_T(".png")) != 0) outfile+=_T(".png");
			break;
		}

		_wfopen_s(&fP, outfile, _T("rb"));
		if (NULL != fP) {
			fclose(fP);
			if (cmdFileIn == NULL) {
				if (IDNO == AfxMessageBox(_T("File exists -- overwrite?"), MB_YESNO)) {
					return;
				}
			}
		}

		if (dlg.m_pOFN->nFilterIndex == 3) {
			// we don't need the conversion below, so just do the work and return here
			HISDEST dst = IS40_OpenFileDest(outfile, FALSE);
			if (NULL != dst) {
				if (0 == IS40_WritePNG(dst, buf8, 280, 192, 280, 8, 256, winpal, 3, 0.0, NULL, 0)) {
					AfxMessageBox(_T("Error while writing file!"));
				}
				IS40_CloseDest(dst);
			} else {
				AfxMessageBox(_T("Failed to save file!"));
			}
			return;
		}

		// we now get to process buf8, converting palette pixels into apple 2 data
		_wfopen_s(&fP, outfile, _T("wb"));
		if (NULL == fP) {
			AfxMessageBox(_T("Failed to open image file"));
			return;
		}

		// Now process the image
		printf("Convert to memory dump...\n");
		OnBnClickedMono();		// now it's the right data in pbuf, but it's not properly interleaved

		// because there are holes in HGR memory space, we need to copy into a new block
		unsigned char outbuf[8192];
		memset(outbuf, 0, sizeof(outbuf));
			
		int nOffIn=0;
		for (int outer1=0; outer1<3; outer1++) {
			for (int outer2=0; outer2<8; outer2++) {
				for (int outer3=0; outer3<8; outer3++) {
					int nOffOut=outer3*1024 + outer2*128 + outer1*40;	// start of line in outbuf
//					int nOffIn=((outer1*64)+(outer2*8)+(outer3))*280;	// start of line in pbuf
					memcpy(&outbuf[nOffOut], &pbuf[nOffIn], 40);
					nOffIn+=40;
				}
			}
		}
		
		switch (dlg.m_pOFN->nFilterIndex) {
		case 1:
			// create a disk structure - not bootable, just our one little file on it
			// this is because tools for Apple2 file manipulation on Windows are very rare
			debug(_T("Writing new DSK image with one file\n"));
			// Basic structure:
			// 35 tracks (0-34), each with 16 sectors of 256 bytes.
			// DOS loads on tracks 0-2 (12k)
            fwrite(dos33, 1, 12*1024, fP);

            // write the 1 sector HELLO program immediately after it (sector 48)
            // we don't store all 256 bytes, so we use a little buffer first
            {
                char buf[256];
                memset(buf, 0, sizeof(buf));
                memcpy(buf, hello, sizeof(hello));
                fwrite(buf, 1, 256, fP);
            }
            
            // The catalog track is on track 17, and files usually start 
            // immediately after it on track 18.
			// That will be our goal.
			// So next, 14 tracks of empty (tracks 3-16)
			for (int idx=0; idx<14*16*256-256; idx++) {
				fputc(0x00, fP);
			}

			// now write a VTOC
			fputc(0, fP);			// unused
			fputc(17, fP);			// first catalog track
			fputc(15, fP);			// first catalog sector
			fputc(3, fP);			// DOS 3.3
			fputc(0, fP);			// skip
			fputc(0, fP);			// skip
			fputc(254, fP);			// volume number
			for (int idx=7; idx<0x27; idx++) {
				fputc(0, fP);		// skip
			}
			fputc(122, fP);			// track/sector list size per sector
			for (int idx=0x28; idx<0x30; idx++) {
				fputc(0, fP);		// skip
			}
			fputc(19, fP);			// last track with allocated sectors (image uses tracks 18-19)
			fputc(1, fP);			// direction of allocation (+1)
			fputc(0, fP);			// skip
			fputc(0, fP);			// skip
			fputc(35, fP);			// number of tracks
			fputc(16, fP);			// number of sectors
			fputc(0, fP);			// bytes per sector LSB
			fputc(1, fP);			// bytes per sector MSB (256)
			for (int idx=0x38; idx<0x7c; idx+=4) {
				fputc(0xff, fP);	// sector bitmap tracks 0-16
				fputc(0xff, fP);	// sector bitmap tracks 0-16 (we lie and say ALL full)
				fputc(0, fP);		// sector bitmap tracks 0-16
				fputc(0, fP);		// sector bitmap tracks 0-16
			}
			fputc(0x7f, fP);		// track 17, sectors 0 and 15 only used
			fputc(0xfe, fP);
			fputc(0, fP);
			fputc(0, fP);
			for (int idx=0x80; idx<0x88; idx++) {
				fputc(0, fP);		// track 18 and 19 fully used
			}
			fputc(0xff, fP);		// track 20, sector 0 only used
			fputc(0xfe, fP);
			fputc(0, fP);
			fputc(0, fP);
			for (int idx=0x8c; idx<0xc4; idx+=4) {	// tracks up to 35
				fputc(0xff, fP);	// sector bitmap
				fputc(0xff, fP);	// sector bitmap
				fputc(0, fP);		// sector bitmap
				fputc(0, fP);		// sector bitmap
			}
			for (int idx=0xc4; idx<0x100; idx++) {
				fputc(0, fP);		// rest of sector unused
			}
			// write 14 blank catalog, each pointing back except 0
			for (int idx=0; idx<14; idx++) {
				fputc(0, fP);		// skip
				if (idx==0) {
					fputc(0, fP);	// no next link
					fputc(0, fP);
				} else {
					fputc(17, fP);		// track of next catalog sector
					fputc(idx, fP);	// sector
				}
				for (int idx2=0; idx2<253; idx2++) {
					fputc(0, fP);		// blank
				}
			}
			// write the catalog sector
			fputc(0, fP);			// skip
			fputc(17, fP);			// track of next catalog sector
			fputc(14, fP);			// sector
			for (int idx=0x03; idx<0x0b; idx++) {
				fputc(0, fP);		// skip
			}
            // first entry (HELLO)
			fputc(20, fP);			// track/sector list on track 20 (yeah, this is imperfect, but makes my code easier)
			fputc(1, fP);			// sector of track/sector list
			fputc(0x02, fP);		// APPLESOFT file type
			fputc('H'+0x80, fP);	// filename - high bit set on each character 
			fputc('E'+0x80, fP);	
			fputc('L'+0x80, fP);	
			fputc('L'+0x80, fP);	
			fputc('O'+0x80, fP);	
			for (int idx=5; idx<30; idx++) {
				fputc(' '+0x80, fP);	// pad filename
			}
			fputc(2, fP);			// size in sectors LSB
			fputc(0, fP);			// size MSB
            // second entry (HGRFILE)
			fputc(20, fP);			// track/sector list on track 20 (yeah, this is imperfect, but makes my code easier)
			fputc(0, fP);			// sector of track/sector list
			fputc(0x04, fP);		// BINARY file type
			fputc('H'+0x80, fP);	// filename - high bit set on each character 
			fputc('G'+0x80, fP);	
			fputc('R'+0x80, fP);	
			fputc('F'+0x80, fP);	
			fputc('I'+0x80, fP);	
			fputc('L'+0x80, fP);	
			fputc('E'+0x80, fP);	
			for (int idx=7; idx<30; idx++) {
				fputc(' '+0x80, fP);	// pad filename
			}
			fputc(33, fP);			// size in sectors LSB
			fputc(0, fP);			// size MSB
            // padding
			for (int idx=0x51; idx<0x100; idx++) {
				fputc(0, fP);		// empty rest of the sector
			}

            // HGRFILE
			// first the 8 byte header - address and length as 16-bit little endian
			fputc(0x00, fP);		// start LSB
			fputc(0x20, fP);		// start MSB
			fputc(0xf8, fP);		// length LSB
			fputc(0x1f, fP);		// length MSB
			// now just dump the data out - this fills tracks 18 and 19 (only need to write 0x1ff8 bytes, rest is padding)
			fwrite(outbuf, 1, 0x1ffc, fP);

			// now write the T/S list for track 20 sector 0 (HGRFILE)
			fputc(0, fP);			// skip
			fputc(0, fP);			// no next T/S track
			fputc(0, fP);			// no next sector
			fputc(0, fP);			// skip
			fputc(0, fP);			// skip
			fputc(0, fP);			// offset of sector list LSB (0 means 0x0c)
			fputc(0, fP);			// offset of sector list MSB
			for (int idx=0x07; idx<0x0c; idx++) {
				fputc(0, fP);		// skip
			}
            // write the actual track/sector list (32 sectors)
			for (int idx=18; idx<20; idx++) {	// track count
				for (int idx2=0; idx2<16; idx2++) {	// sector count
					fputc(idx, fP);	// track
					fputc(idx2,fP);	// sector
				}
			}
			// pad the rest of the sector
			for (int idx=0x4c; idx<0x100; idx++) {
				fputc(0, fP);
			}

			// now write the T/S list for track 20 sector 1 (HELLO)
			fputc(0, fP);			// skip
			fputc(0, fP);			// no next T/S track
			fputc(0, fP);			// no next sector
			fputc(0, fP);			// skip
			fputc(0, fP);			// skip
			fputc(0, fP);			// offset of sector list LSB (0 means 0x0c)
			fputc(0, fP);			// offset of sector list MSB
			for (int idx=0x07; idx<0x0c; idx++) {
				fputc(0, fP);		// skip
			}
            // write the actual track/sector list (1 sector)
            fputc(3, fP);   // track 3
            fputc(0, fP);   // sector 0

            // pad the rest of the sector
			for (int idx=0x0E; idx<0x100; idx++) {
				fputc(0, fP);
			}

			// and blank the rest of the disk
			for (int idx=0; idx<(15*16*256)-512; idx++) {	// -512 for sectors 20:(0-1)
				fputc(0, fP);
			}
			break;

		default:
		case 2:
			// write the block
			debug(_T("Writing HGR file.\n"));
			fwrite(outbuf, 1, 8192, fP);	// only 0x1ff8 bytes are needed because of the ending memory hole
			break;
		}
		fclose(fP);
		fP=NULL;

		// reload the image
		OnBnClickedReload();
	}
}

void CTIPicViewDlg::OnDropFiles(HDROP hDropInfo) 
{
	wchar_t szFile[1024];

	ZeroMemory(szFile, sizeof(szFile));

	DragQueryFile(hDropInfo, 0, szFile, 1024);
	DragFinish(hDropInfo);

	if (wcslen(szFile)) {
		LaunchMain(2,szFile);
	}
}

void CTIPicViewDlg::OnBnClickedButton3()
{
	bool bShift=false;

	heightoffset--;
	if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
		heightoffset--;		// shift moves by one more
		bShift=true;
	}
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
		heightoffset-=3;	// ctrl moves by three more
		if (bShift) {
			// if it was both, add another 4 to make 8
			heightoffset-=4;
		}
	}
	LaunchMain(1,"");
}

void CTIPicViewDlg::OnBnClickedButton5()
{
	bool bShift=false;

	heightoffset++;
	if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
		heightoffset++;		// shift moves by one more
		bShift=true;
	}
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
		heightoffset+=3;	// ctrl moves by three more
		if (bShift) {
			// if it was both, add another 4 to make 8
			heightoffset+=4;
		}
	}
	LaunchMain(1,"");
}

void CTIPicViewDlg::EnableItem(int Ctrl) {
	CWnd *pWnd = GetDlgItem(Ctrl);
	if (NULL != pWnd) {
		pWnd->EnableWindow(TRUE);
	}
}
void CTIPicViewDlg::DisableItem(int Ctrl) {
	CWnd *pWnd = GetDlgItem(Ctrl);
	if (NULL != pWnd) {
		pWnd->EnableWindow(FALSE);
	}
}

void CTIPicViewDlg::ResetControls() {
	// set all controls to enabled, then filter in the perceptual button just to centralize that
	// only controls that get enabled/disabled are listed
	static int nControls[] = {
		IDC_EDIT2, IDC_EDIT1, IDC_EDIT3, IDC_EDIT4, IDC_PERPR, IDC_PERPG, IDC_PERPB, 
		IDC_ERRMODE, IDC_RADIO1, IDC_RADIO2, IDC_LUMA
	};

	for (int idx=0; idx<sizeof(nControls)/sizeof(nControls[0]); idx++) {
		EnableItem(nControls[idx]);
	}
	if (!g_Perceptual) {
		DisableItem(IDC_PERPR);
		DisableItem(IDC_PERPG);
		DisableItem(IDC_PERPB);
	} else {
		DisableItem(IDC_LUMA);
	}
}

// Perceptual color
void CTIPicViewDlg::OnBnClickedPercept()
{
	CButton *pCtrl=(CButton*)GetDlgItem(IDC_PERCEPT);
	if (pCtrl) {
		if (pCtrl->GetCheck() == BST_CHECKED) {
			g_Perceptual=true;
		} else {
			g_Perceptual=false;
		}
	}
	ResetControls();
}

void CTIPicViewDlg::LaunchMain(int mode, CString pFile) {
	CollectData();

	PIXA=m_pixelA;
	PIXB=m_pixelB;
	PIXC=m_pixelC;
	PIXD=m_pixelD;
	PIXE=m_pixelE;
	PIXF=m_pixelF;
	g_orderSlide = m_nOrderSlide;
	g_nFilter = m_nFilter;
	g_nPortraitMode = m_nPortraitMode;
	g_AccumulateErrors = (m_ErrMode == 0) ? 0 : 1;
	g_MaxColDiff = m_MaxColDiff;
	g_PercepR = m_PercepR / 100.0;
	g_PercepG = m_PercepG / 100.0;
	g_PercepB = m_PercepB / 100.0;
	g_LumaEmphasis = m_Luma / 100.0;
	g_Gamma = m_Gamma / 100.0;
	g_Grey = (m_CtrlGrey.GetCheck() == BST_CHECKED);

	if (InterlockedCompareExchange(&cs, 1, 0) == 1) {
		printf("Error locking - already processing\n");
	} else {
		maincode(mode, pFile, m_nOrderSlide);
		InterlockedExchange(&cs, 0);
	}
	GreenMode=false;
}

void CTIPicViewDlg::OnBnClickedReload()
{
	LaunchMain(1,"");
}

void CTIPicViewDlg::MakeConversionModeValid() {
    // this has to do with changing conversion output modes in the TI version
}

bool CTIPicViewDlg::ResetToValidConversion() {
    // this has to do with changing conversion output modes in the TI version
	return true;
}

// old ErrDistDlg
void CTIPicViewDlg::PrepareData()
{
	CButton *pBtn;
	pBtn = (CButton*)GetDlgItem(IDC_RADIO1);
	if (pBtn) pBtn->SetCheck(BST_CHECKED);
	pBtn = (CButton*)GetDlgItem(IDC_RADIO2);
	if (pBtn) pBtn->SetCheck(BST_UNCHECKED);

	CButton *pCtrl=(CButton*)GetDlgItem(IDC_PERCEPT);
	if (pCtrl) {
		if (g_Perceptual) {
			pCtrl->SetCheck(BST_CHECKED);
		} else {
			pCtrl->SetCheck(BST_UNCHECKED);
		}
	}

	UpdateData(false);
}

//	... PIX D E
//	 A   B  C
//	     F

// helper for dither controls
void CTIPicViewDlg::EnableDitherCtrls(bool enable) {
	if (enable) {
		EnableItem(IDC_EDIT1);
		EnableItem(IDC_EDIT2);
		EnableItem(IDC_EDIT3);
		EnableItem(IDC_EDIT4);
		EnableItem(IDC_EDIT5);
		EnableItem(IDC_EDIT6);
		DisableItem(IDC_ORDERSLIDE);
	} else{
		DisableItem(IDC_EDIT1);
		DisableItem(IDC_EDIT2);
		DisableItem(IDC_EDIT3);
		DisableItem(IDC_EDIT4);
		DisableItem(IDC_EDIT5);
		DisableItem(IDC_EDIT6);
		EnableItem(IDC_ORDERSLIDE);
	}
}

// Floyd-Steinberg Dither
void CTIPicViewDlg::OnBnClickedFloyd()
{
	UpdateData(true);
	m_pixelA=3;
	m_pixelB=5;
	m_pixelC=1;
	m_pixelD=7;
	m_pixelE=0;
	m_pixelF=0;
	UpdateData(false);
	untoggleOrdered();
}

// atkinson dither
void CTIPicViewDlg::OnBnClickedAtkinson()
{
	UpdateData(true);
	m_pixelA=1;
	m_pixelB=2;
	m_pixelC=2;
	m_pixelD=2;
	m_pixelE=1;
	m_pixelF=1;
	UpdateData(false);
	untoggleOrdered();
}

void CTIPicViewDlg::OnBnClickedPattern()
{
	UpdateData(true);
	m_pixelA=0;
	m_pixelB=8;
	m_pixelC=0;
	m_pixelD=8;
	m_pixelE=0;
	m_pixelF=0;
	UpdateData(false);
	untoggleOrdered();
}

void CTIPicViewDlg::OnBnClickedDiag()
{
	UpdateData(true);
	m_pixelA=1;
	m_pixelB=3;
	m_pixelC=2;
	m_pixelD=3;
	m_pixelE=1;
	m_pixelF=1;
	UpdateData(false);
	untoggleOrdered();
}

void CTIPicViewDlg::OnBnClickedNodither()
{
	UpdateData(true);
	m_pixelA=0;
	m_pixelB=0;
	m_pixelC=0;
	m_pixelD=0;
	m_pixelE=0;
	m_pixelF=0;
	UpdateData(false);
	untoggleOrdered();
}

void CTIPicViewDlg::untoggleOrdered() 
{
	SendDlgItemMessage(IDC_ORDERED, BM_SETCHECK, BST_UNCHECKED, 0);
	SendDlgItemMessage(IDC_ORDERED2, BM_SETCHECK, BST_UNCHECKED, 0);
	SendDlgItemMessage(IDC_ORDERED3, BM_SETCHECK, BST_UNCHECKED, 0);
	SendDlgItemMessage(IDC_ORDERED4, BM_SETCHECK, BST_UNCHECKED, 0);
	EnableDitherCtrls(TRUE);
	g_OrderedDither = 0;
}

void CTIPicViewDlg::OnBnClickedOrdered()
{
	// this one is a toggle
	SendDlgItemMessage(IDC_ORDERED2, BM_SETCHECK, BST_UNCHECKED, 0);
	SendDlgItemMessage(IDC_ORDERED3, BM_SETCHECK, BST_UNCHECKED, 0);
	SendDlgItemMessage(IDC_ORDERED4, BM_SETCHECK, BST_UNCHECKED, 0);
	if (BST_CHECKED == SendDlgItemMessage(IDC_ORDERED, BM_GETCHECK, 0, 0)) {
		EnableDitherCtrls(FALSE);
		g_OrderedDither = 1;
		g_MapSize = 2;
	} else {
		EnableDitherCtrls(TRUE);
		g_OrderedDither = 0;
	}
}


void CTIPicViewDlg::OnBnClickedOrdered2()
{
	// this one is a toggle but keeps the dither up
	SendDlgItemMessage(IDC_ORDERED, BM_SETCHECK, BST_UNCHECKED, 0);
	SendDlgItemMessage(IDC_ORDERED3, BM_SETCHECK, BST_UNCHECKED, 0);
	SendDlgItemMessage(IDC_ORDERED4, BM_SETCHECK, BST_UNCHECKED, 0);
	if (BST_CHECKED == SendDlgItemMessage(IDC_ORDERED2, BM_GETCHECK, 0, 0)) {
		EnableDitherCtrls(TRUE);
		g_OrderedDither = 2;
		g_MapSize = 2;

		UpdateData(true);
		m_pixelA=1;
		m_pixelB=2;
		m_pixelC=2;
		m_pixelD=2;
		m_pixelE=0;
		m_pixelF=0;
		UpdateData(false);

	} else {
		EnableDitherCtrls(TRUE);
		g_OrderedDither = 0;
	}
}

void CTIPicViewDlg::OnBnClickedOrdered3()
{
	// this one is a toggle
	SendDlgItemMessage(IDC_ORDERED, BM_SETCHECK, BST_UNCHECKED, 0);
	SendDlgItemMessage(IDC_ORDERED2, BM_SETCHECK, BST_UNCHECKED, 0);
	SendDlgItemMessage(IDC_ORDERED4, BM_SETCHECK, BST_UNCHECKED, 0);
	if (BST_CHECKED == SendDlgItemMessage(IDC_ORDERED3, BM_GETCHECK, 0, 0)) {
		EnableDitherCtrls(FALSE);
		g_OrderedDither = 1;
		g_MapSize = 4;
	} else {
		EnableDitherCtrls(TRUE);
		g_OrderedDither = 0;
	}
}

void CTIPicViewDlg::OnBnClickedOrdered4()
{
	// this one is a toggle but keeps the dither up
	SendDlgItemMessage(IDC_ORDERED, BM_SETCHECK, BST_UNCHECKED, 0);
	SendDlgItemMessage(IDC_ORDERED2, BM_SETCHECK, BST_UNCHECKED, 0);
	SendDlgItemMessage(IDC_ORDERED3, BM_SETCHECK, BST_UNCHECKED, 0);
	if (BST_CHECKED == SendDlgItemMessage(IDC_ORDERED4, BM_GETCHECK, 0, 0)) {
		EnableDitherCtrls(TRUE);
		g_OrderedDither = 2;
		g_MapSize = 4;

		UpdateData(true);
		m_pixelA=1;
		m_pixelB=2;
		m_pixelC=2;
		m_pixelD=2;
		m_pixelE=0;
		m_pixelF=0;
		UpdateData(false);

	} else {
		EnableDitherCtrls(TRUE);
		g_OrderedDither = 0;
	}
}


void CTIPicViewDlg::OnBnClickedPerpreset()
{
	UpdateData(true);
	m_PercepR = 30;
	m_PercepG = 52;		// no longer 59 - reduced to make up the blue difference
	m_PercepB = 18;		// no longer 11 - changed to reduce blue in dark areas
	UpdateData(false);
}

void CTIPicViewDlg::CollectData()
{
	UpdateData(true);
}
void CTIPicViewDlg::OnBnClickedMono()
{
	// convert the image to mono - this will be the actual bit patterns that we store
	// so it's used in the save function (the button is just for test purposes)
	unsigned char *pOut = pbuf;
	unsigned char *pIn = buf8;
	unsigned char x=0;

	if (GreenMode) return;		// already converted

	// for kicks, we'll write in green back out to buf8 (so we can view it), palette color 1
	for (int row=0; row<192; row++) {
		for (int col=0; col<280; col+=7) {
			unsigned char work[7];
			unsigned char out;
			unsigned char col1,col2;
			
			memcpy(work, pIn, 7);		// get the 7 pixels - at this point, only color and position matters!

			// sanity test - make sure all 7 pixels are in the same color group
			for (int idx=0; idx<6; idx++) {
				if ((work[idx]&0x04) != (work[idx+1]&0x04)) {
					AfxMessageBox(_T("Error in color conversion - mismatched color set in byte"));
					return;
				}
			}

			out=(work[0]&0x04)<<5;		// make high bit
			// calculate masks
			if ((col/7)&1) {
				// odd, so 1 comes first (but we start at 0x40, not 0x80)
				col1=0x55;
				col2=0xaa;
			} else {
				// even, 2 comes first
				col1=0xaa;
				col2=0x55;
			}
						
			// process the 7 pixels
			unsigned char mask=0x40;	// masking the bitmask 
			unsigned char maskout=0x01;	// goes the other way!
			for (int idx=0; idx<7; idx++) {
				work[idx]&=0x03;
				if (work[idx]==0x03) {
					// white, so it stays on regardless
					work[idx]=1;
				} else if (work[idx]==1) {
					work[idx]=(col1&mask)?1:0;
				} else if (work[idx]==2) {
					work[idx]=(col2&mask)?1:0;
				} else {
					work[idx]=0;
				}
				if (work[idx]) out|=maskout;
				mask>>=1;
				maskout<<=1;
			}

			memcpy(pIn, work, 7);		// get the 7 pixels - at this point, only color and position matters!
			pIn+=7;
			*(pOut++)=out;
		}
	}

	// and it's converted
	GreenMode=true;

	// is that really all it takes??
	InvalidateRect(NULL);
}


void CTIPicViewDlg::OnBnClickedGreen()
{
	OnBnClickedMono();
}


void CTIPicViewDlg::OnTimer(UINT_PTR nIDEvent)
{
	CDialog::OnTimer(nIDEvent);

	// don't even check if the flag is set - we set it to '2' here
	// maincode will set it to '1'
	if (InterlockedCompareExchange(&cs, 1, 2) != 0) {
		return;
	}

	if (NULL != pSharedPointer) {
		// this isn't very atomic, but, it's never supposed to
		// change after the first time it's set.
		wchar_t buf[256];
		memcpy(buf, pSharedPointer, sizeof(buf));
//		InterlockedExchange((LONG*)pSharedPointer, 0);	// this only works as long as it takes us longer to load the image as the other app needs to scan.
														// TODO: we could do better.
		if (buf[0] != '\0') {
			if (0 != wcscmp(szLastFilename, buf)) {
				// don't bother if it's the same as we already have
				LaunchMain(2, buf);
			}
		}
	}
}


