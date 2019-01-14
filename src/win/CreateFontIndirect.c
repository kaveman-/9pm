#include "win.h"

#undef CreateFontIndirect
HFONT  WINAPI
CreateFontIndirect(LOGFONT *lf)
{
	LOGFONTA lfa;

	if(win_useunicode)
		return CreateFontIndirectW(lf);

	memmove(&lfa, lf, (long)&((LOGFONT*)0)->lfFaceName[0]);
	win_wstr2utf(lfa.lfFaceName, LF_FACESIZE, lf->lfFaceName);
	return CreateFontIndirectA(&lfa);
}
