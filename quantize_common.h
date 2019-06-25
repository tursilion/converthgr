// The following parts are the same for all the quantize methods

extern wchar_t *cmdFileIn;
extern bool fVerbose;
extern MYRGBQUAD palinit16[256];
extern double g_thresholdMap2x2[2][2];
extern double g_thresholdMap4x4[4][4];

// macro to change the masking for the threshold map
#define MAPSEEK(x) (x)&maskval

// pIn is a 24-bit RGB image
// pOut is an 8-bit palettized image, with the palette in 'pal'
// pOut contains a multicolor mapped image if half-multicolor is on
// CurrentBest is the current closest value and is used to abort a search without
// having to check all 8 pixels for a small speedup when the color is close
// note that this function is never compiled -- it is renamed by each various function define
void quantize_common(BYTE* pRGB, BYTE* p8Bit, double darkenval, int mapSize) {
	int row,col;
	// local temporary usage - adjusted map
	double g_thresholdMap2[4][4];
	double nDesiredPixels[7][3];		// one byte gives us 7 pixels to search
#ifdef ERROR_DISTRIBUTION
	double fPIXA, fPIXB, fPIXC, fPIXD, fPIXE, fPIXF;
#endif
	double (*thresholdMap)[4];
	int maskval;

	if (mapSize == 2) {
		maskval = 1;
	} else if (mapSize == 4) {
		maskval = 3;
	} else {
		printf("Bad map size %d\n", mapSize);
		return;
	}

	// update threshold map 2 for the darken value
	for (int i1=0; i1<mapSize; i1++) {
		for (int i2=0; i2<mapSize; i2++) {
			// so it's a slider now - but the original algorithm /does/ center around zero
			// so normally this should be at 8, halfway
			// Since this is a fraction it doesn't matter if it's 8x8 or 4x4
			if (mapSize == 2) {
				g_thresholdMap2[i1][i2] = g_thresholdMap2x2[i1][i2] - (darkenval/16.0);
			} else {
				g_thresholdMap2[i1][i2] = g_thresholdMap4x4[i1][i2] - (darkenval/16.0);
			}
		}
		thresholdMap = g_thresholdMap2;
	}

	// timing report
	time_t nStartTime, nEndTime;
	time(&nStartTime);

#ifdef ERROR_DISTRIBUTION
	// convert the integer dither ratios just once up here
	fPIXA = (double)PIXA/16.0;
	fPIXB = (double)PIXB/16.0;
	fPIXC = (double)PIXC/16.0;
	fPIXD = (double)PIXD/16.0;
	fPIXE = (double)PIXE/16.0;
	fPIXF = (double)PIXF/16.0;
#endif

	// create some workspace
#ifdef ERROR_DISTRIBUTION
	double *pError = (double*)malloc(sizeof(double)*283*194*3);	// error map, 3 colors, includes 1 pixel border on all sides but top and 2 on bottom (so first entry is x=-1, y=0, and each row is 562 pixels wide)
	for (int idx=0; idx<282*194*3; idx++) {
		pError[idx]=0.0;
	}
#endif

	// go ahead and get to work
	for (row = 0; row < 192; row++) {
		MSG msg;
		if (row%8 == 0) {
			// this one debug, I don't want to go to outputdebugstring for performance reasons...
			if ((cmdFileIn == NULL) || (fVerbose)) {
				printf("Processing row %d...\r", row);
			}

			// keep windows happy
			if (NULL == cmdFileIn) {
				if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE|PM_NOYIELD)) {
					AfxGetApp()->PumpMessage();
				}
			}
		}

#ifdef ERROR_DISTRIBUTION
		// divisor for the error value. Three errors are added together except for the left
		// column (only 2), and top row (only 1). The top left pixel has none, but no divisor
		// is needed since the stored value is also zero.
		// there is no notable performance difference to removing this for the accumulate errors mode
		double nErrDivisor = 3.0;
		if ((row == 0)||(g_AccumulateErrors)) nErrDivisor=1.0;
#endif

		for (col = 0; col < 280; col+=7) {
			// address of byte in the RGB image (3 bytes per pixel)
			BYTE *pInLine = pRGB + (row*280*3) + (col*3);
			// address of byte in the 8-bit image (1 byte per pixel)
			BYTE *pOutLine = p8Bit + (row*280) + col;

#ifdef ERROR_DISTRIBUTION
			// address of entry in the error table (3 doubles per pixel and larger array)
			double *pErrLine = pError + (row*282*3) + ((col+1)*3);
#endif

			// our first job is to get the desired pixel pattern for this block of 7 pixels
			// This takes the original image, and adds in the data from the error map. We do
			// all matching in double mode now.
			for (int c=0; c<7; c++, pInLine+=3
#ifdef ERROR_DISTRIBUTION
				, pErrLine+=3
#endif
				) {
				nDesiredPixels[c][0] = (double)(*pInLine);		// red
				nDesiredPixels[c][1] = (double)(*(pInLine+1));	// green
				nDesiredPixels[c][2] = (double)(*(pInLine+2));	// blue

				// don't dither if the desired color is pure black or white (helps reduce error spread)	
				if ((nDesiredPixels[c][0]==0)&&(nDesiredPixels[c][1]==0)&&(nDesiredPixels[c][2]==0)) continue;
				if ((nDesiredPixels[c][0]==0xff)&&(nDesiredPixels[c][1]==0xff)&&(nDesiredPixels[c][2]==0xff)) continue;

#ifdef QUANTIZE_ORDERED
				nDesiredPixels[c][0]+=nDesiredPixels[c][0]*thresholdMap[MAPSEEK(c)][MAPSEEK(row)];
				nDesiredPixels[c][1]+=nDesiredPixels[c][1]*thresholdMap[MAPSEEK(c)][MAPSEEK(row)];
				nDesiredPixels[c][2]+=nDesiredPixels[c][2]*thresholdMap[MAPSEEK(c)][MAPSEEK(row)];
#endif
#ifdef ERROR_DISTRIBUTION
				// add in the error table for these pixels
				// Technically pixel 0 on each row except 0 should be /2, this will do /3, but that is close enough
				nDesiredPixels[c][0]+=(*pErrLine)/nErrDivisor;		// we don't want to clamp - that induces color shift
				nDesiredPixels[c][1]+=(*(pErrLine+1))/nErrDivisor;
				nDesiredPixels[c][2]+=(*(pErrLine+2))/nErrDivisor;
#endif
			}

			// tracks the best distance (starts out bigger than expected)
			double nBestDistance = 256.0*256.0*256.0*256.0*256.0*256.0;
			int nBestPat=0;
			int nLastPix=0;
			if (col > 0) {
				nLastPix=*(pOutLine-1);		// color from last 7-bit group
				// all we care about is, was it ON or OFF, not what color it ended up. White is on for sure.
				// black is off for sure. But the other colors depend on column.
				if ((col/7)&1) {
					// we are on odd, last column was even, so color 2 is the last bit
					if (nLastPix&2) {
						nLastPix=1;
					} else {
						nLastPix=0;
					}
				} else {
					// we are on even, last column was odd, so color 1 is the last bit
					if (nLastPix&1) {
						nLastPix=1;
					} else {
						nLastPix=0;
					}
				}
			}
#ifdef ERROR_DISTRIBUTION
			double nErrorOutput[6];				// saves the error output for the next horizontal pixel (vertical only calculated on the best match)
			// zero the error output (will be set to the best match only)
			for (int i1=0; i1<6; i1++) {
				nErrorOutput[i1]=0.0;
			}
#endif

			// start searching for the best match
			int pat=rand()%256;
			for (int idx=0; idx<256; idx++, pat=(++pat)&0xff) {				// pixel pattern (random start to scatter patterns)
				double t1,t2,t3;
				double nCurDistance;
				int nMask=0x40;			// skip the MSb
				int odd,even;

#ifdef ERROR_DISTRIBUTION
				double tmpR, tmpG, tmpB, farR, farG, farB;
				tmpR=0.0;
				tmpG=0.0;
				tmpB=0.0;
				farR=0.0;
				farG=0.0;
				farB=0.0;
#endif

				// begin the search
				nCurDistance = 0.0;

				for (int bit=0; bit<7; bit++) {					// only 7 bits to analyze
					int nCol;
					// which color to use - this depends on bit position and the previous pixel
					// it also affects the next pixel! So we check both.

					// first, work out which of the two 'sets' to use
					if ((col/7)&1) {
						// odd numbered column, we are working with colors 4-7 unless the MSB is set
						if (pat&0x80) {
							nCol=0;
						} else {
							nCol=4;		// set mask
						}
						odd=2;
						even=1;
					} else {
						// even numbered column, colors 0-3 unless MSB is set
						if (pat&0x80){ 
							nCol=4;
						} else {
							nCol=0;
						}
						odd=1;
						even=2;
					}

					if (pat&nMask) {
						// this bit is set. It can be either color or white, depending on the previous and next bits
						// assume color first
						if (bit&1) {
							nCol|=odd;
						} else {
							nCol|=even;	// 2 is first horizontally
						}
						// now check for white
						if ((bit==0)&&(nLastPix)) nCol|=3;		// previous pixel was set
						if ((bit>0)&&(pat&(nMask<<1))) nCol|=3;		// previous pixel was set
						if ((bit<7)&&(pat&(nMask>>1))) nCol|=3;		// next pixel is set
						// TODO: we can't really check next pixel, we haven't done it yet! This is a hole.
					} else {
						// this bit is NOT set. But it may be a color pixel if the previous
						// pixel is set. We do not need to check the next pixel in this case
						if ((bit>0)&&(pat&(nMask<<1))) {
							// reverse it so we get the last pixel's color
							if (bit&1) {
								nCol|=even;
							} else {
								nCol|=odd;
							}
						}
						if ((bit == 0) && (nLastPix)) {
							// same deal, but for previous word
							if (bit&1) {
								nCol|=even;
							} else {
								nCol|=odd;
							}
						}
					}

					// calculate actual error for this pixel
					// Note: wrong divisor for pixel 0 on each row except 0 (should be 2 but will be 3), 
					// but we'll live with it for now
					// update the total error
					double r,g,b;
					// get RGB
					r=nDesiredPixels[bit][0];
					g=nDesiredPixels[bit][1];
					b=nDesiredPixels[bit][2];

#ifdef ERROR_DISTRIBUTION
					// get RGB
					r+=tmpR/nErrDivisor;
					g+=tmpG/nErrDivisor;
					b+=tmpB/nErrDivisor;
#endif
					// we need the RGB diffs for the error diffusion anyway
					t1=r-pal[nCol][0];
					t2=g-pal[nCol][1];
					t3=b-pal[nCol][2];

#ifdef QUANTIZE_PERCEPTUAL
					nCurDistance+=(t1*t1)*g_PercepR+(t2*t2)*g_PercepG+(t3*t3)*g_PercepB;
#else
					nCurDistance+=yuvpaldist(r,g,b, nCol);
#endif

					if (nCurDistance >= nBestDistance) {
						// no need to search this one farther
						break;
					}

#ifdef ERROR_DISTRIBUTION
					// update expected horizontal error based on new actual error
					tmpR=t1*fPIXD;
					tmpG=t2*fPIXD;
					tmpB=t3*fPIXD;

					// migrate the far pixel in
					tmpR+=farR;
					tmpG+=farG;
					tmpB+=farB;

					// calculate the next far pixel
					farR=t1*fPIXE;
					farG=t2*fPIXE;
					farB=t3*fPIXE;
#endif

					// next bit position
					nMask>>=1;
				}

				// this byte is tested, did we find a better match?
				if (nCurDistance < nBestDistance) {
					// we did! So save the data off
					nBestDistance = nCurDistance;
#ifdef ERROR_DISTRIBUTION
					nErrorOutput[0] = tmpR;
					nErrorOutput[1] = tmpG;
					nErrorOutput[2] = tmpB;
					nErrorOutput[3] = farR;
					nErrorOutput[4] = farG;
					nErrorOutput[5] = farB;
#endif
					nBestPat=pat;
				}
			}

			// at this point, we have a best match for this byte
#ifdef ERROR_DISTRIBUTION
			// first we update the error map to the right with the error output
			*(pErrLine)+=nErrorOutput[0];
			*(pErrLine+1)+=nErrorOutput[1];
			*(pErrLine+2)+=nErrorOutput[2];
			*(pErrLine+3)+=nErrorOutput[3];
			*(pErrLine+4)+=nErrorOutput[4];
			*(pErrLine+5)+=nErrorOutput[5];

			// point to the next line, same x pixel as the first one here
			pErrLine += (282*3) - (7*3);
#endif

			// now we have to output the pixels AND update the error map
			int nMask = 0x40;
			int odd,even;
			for (int bit=0; bit<7; bit++) {
				int nCol;
#ifdef ERROR_DISTRIBUTION
				double err;
#endif

				// first, work out which of the two 'sets' to use
				if ((col/7)&1) {
					// odd numbered column, we are working with colors 4-7 unless the MSB is set
					if (nBestPat&0x80) {
						nCol=0;
					} else {
						nCol=4;		// set mask
					}
					odd=2;
					even=1;
				} else {
					// even numbered column, colors 0-3 unless MSB is set
					if (nBestPat&0x80){ 
						nCol=4;
					} else {
						nCol=0;
					}
					odd=1;
					even=2;
				}

				if (nBestPat&nMask) {
					// this bit is set. It can be either color or white, depending on the previous and next bits
					// assume color first
					if (bit&1) {
						nCol|=odd;
					} else {
						nCol|=even;	// 2 is first horizontally
					}
					// now check for white
					if ((bit==0)&&(nLastPix)) {
						nCol|=3;				// previous pixel was set
						// since this pixel is going white, that previous one is now white too,
						// so ret-con it. This is kind of a flaw in the algorithm- it is hard to
						// look ahead. But if we don't change the previous output then the final
						// image will be incorrect, and that at least needs to be right.
						*(pOutLine-1)|=0x03;
					}
					if ((bit>0)&&(nBestPat&(nMask<<1))) nCol|=3;		// previous pixel was set
					if ((bit<7)&&(nBestPat&(nMask>>1))) nCol|=3;		// next pixel is set
				} else {
					// this bit is NOT set. But it may be a color pixel if the previous
					// pixel is set. We do not need to check the next pixel in this case
					if ((bit>0)&&(nBestPat&(nMask<<1))) {
						// reverse it so we get the last pixel's color
						if (bit&1) {
							nCol|=even;
						} else {
							nCol|=odd;
						}
					}
					if ((bit == 0) && (nLastPix)) {
						// same deal, but for previous word
						if (bit&1) {
							nCol|=even;
						} else {
							nCol|=odd;
						}
					}
				}
				nMask>>=1;

				*(pOutLine++) = nCol;

#ifdef ERROR_DISTRIBUTION
				// and update the error map 
				err = nDesiredPixels[bit][0] - pal[nCol][0];		// red
				*(pErrLine+(bit*3)-3) += err*fPIXA;
				*(pErrLine+(bit*3)) += err*fPIXB;
				*(pErrLine+(bit*3)+3) += err*fPIXC;
				*(pErrLine+(bit*3)+(258*3)) += err*fPIXF;

				err = nDesiredPixels[bit][1] - pal[nCol][1];		// green
				*(pErrLine+(bit*3)-2) += err*fPIXA;
				*(pErrLine+(bit*3)+1) += err*fPIXB;
				*(pErrLine+(bit*3)+4) += err*fPIXC;
				*(pErrLine+(bit*3)+(258*3)+1) += err*fPIXF;

				err = nDesiredPixels[bit][2] - pal[nCol][2];		// blue
				*(pErrLine+(bit*3)-1) += err*fPIXA;
				*(pErrLine+(bit*3)+2) += err*fPIXB;
				*(pErrLine+(bit*3)+5) += err*fPIXC;
				*(pErrLine+(bit*3)+(258*3)+2) += err*fPIXF;
#endif
			}
		}

		// one line complete - draw it to provide feedback
		if ((NULL != pWnd)&&(NULL == cmdFileIn)) {
			CDC *pCDC=pWnd->GetDC();
			if (NULL != pCDC) {
				int dpi = GetDpiForWindow(pWnd->GetSafeHwnd());
				IS40_StretchDraw8Bit(*pCDC, p8Bit+(row*280), 280, 1, 280, winpal, DPIFIX(XOFFSET), DPIFIX(row*2), DPIFIX(280*2), DPIFIX(2));
				pWnd->ReleaseDC(pCDC);
			}
		}
	}

#ifdef ERROR_DISTRIBUTION
	// finished! clean up
	free(pError);
#endif

	time(&nEndTime);
	debug(_T("Conversion time: %d seconds\n"), nEndTime-nStartTime);
}


