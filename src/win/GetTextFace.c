#include "win.h"

#undef GetTextFace
int  WINAPI
GetTextFace(HDC hdc, int nCount, LPTSTR lpFaceName)
{
	char name[LF_FULLFACESIZE];
	int n;

	if(win_useunicode)
		return GetTextFaceW(hdc, nCount, lpFaceName);

	n = GetTextFaceA(hdc, sizeof(name), name);
	if(n == 0)
		return 0;
	win_utf2wstr(lpFaceName, nCount, name);
	return win_wstrlen(lpFaceName);
}

