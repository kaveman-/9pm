#include "win.h"

typedef struct Callback
{
	FONTENUMPROC	fnc;
	LPARAM		a;
} Callback;

static BOOL PASCAL	callback(ENUMLOGFONTA*, NEWTEXTMETRICA*, DWORD, Callback*);

#undef EnumFontFamilies
int  WINAPI
EnumFontFamilies(HDC hdc, LPCWSTR lpszFamily, FONTENUMPROC lpEnumFontFamProc, LPARAM lParam)
{
	char buf[MAX_PATH], *font;
	Callback cb;

	if(win_useunicode)
		return EnumFontFamiliesW(hdc, lpszFamily, lpEnumFontFamProc, lParam);
	
	if(lpszFamily) {
		win_wstr2utf(buf, MAX_PATH, (Rune*)lpszFamily);
		font = buf;
	} else
		font = 0;

	cb.fnc = lpEnumFontFamProc;
	cb.a = lParam;

	return EnumFontFamiliesA(hdc, font, (void*)callback, (long)&cb);	
}

static BOOL PASCAL
callback(ENUMLOGFONTA *elga, NEWTEXTMETRICA* ntma, DWORD type, Callback *cb)
{
	ENUMLOGFONT elg;
	NEWTEXTMETRIC ntm;
	int i;

   	ntm.tmHeight = ntma->tmHeight;
    	ntm.tmAscent = ntma->tmAscent;
    	ntm.tmDescent = ntma->tmDescent;
    	ntm.tmInternalLeading = ntma->tmInternalLeading;
    	ntm.tmExternalLeading = ntma->tmExternalLeading;
    	ntm.tmAveCharWidth = ntma->tmAveCharWidth;
    	ntm.tmMaxCharWidth = ntma->tmMaxCharWidth;
    	ntm.tmWeight = ntma->tmWeight;
    	ntm.tmOverhang = ntma->tmOverhang;
    	ntm.tmDigitizedAspectX = ntma->tmDigitizedAspectX;
    	ntm.tmDigitizedAspectY = ntma->tmDigitizedAspectY;
    	ntm.tmFirstChar = ntma->tmFirstChar;
    	ntm.tmLastChar = ntma->tmLastChar;
    	ntm.tmDefaultChar = ntma->tmDefaultChar;
	ntm.tmBreakChar = ntma->tmBreakChar;
	ntm.tmItalic = ntma->tmItalic;
	ntm.tmUnderlined = ntma->tmUnderlined;
	ntm.tmStruckOut = ntma->tmStruckOut;
	ntm.tmPitchAndFamily = ntma->tmPitchAndFamily;
	ntm.tmCharSet = ntma->tmCharSet;
	ntm.ntmFlags = ntma->ntmFlags;
 	ntm.ntmSizeEM = ntma->ntmSizeEM;
  	ntm.ntmCellHeight = ntma->ntmCellHeight;
  	ntm.ntmAvgWidth = ntma->ntmAvgWidth;

	/* hack - but strings are at the end */
	elg.elfLogFont = *(LOGFONT*)&elga->elfLogFont;
	for(i=0; i<LF_FACESIZE; i++)
		elg.elfLogFont.lfFaceName[i] = elga->elfLogFont.lfFaceName[i];
	for(i=0; i<LF_FULLFACESIZE; i++)
		elg.elfFullName[i] = elga->elfFullName[i];
	for(i=0; i<LF_FACESIZE; i++)
		elg.elfStyle[i] = elga->elfStyle[i];
	return cb->fnc(&elg, &ntm, type, cb->a);
}
