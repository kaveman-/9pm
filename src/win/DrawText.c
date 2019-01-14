#include "win.h"


#undef DrawText
int WINAPI
DrawText(HDC hDC, LPWSTR lpString, int nCount, LPRECT lpRect, UINT uFormat)
{
	char *s, *p;
	int i, n, r;

	if(win_useunicode)
		return DrawTextW(hDC, lpString, nCount, lpRect, uFormat);

	if(nCount<0)
		n = win_wstrlen(lpString);
	s = malloc(n*UTFmax);
	for(i=0,p=s; i<n; i++)
		p += win_runetochar(p, lpString+i);

	r = DrawTextA(hDC, s, p-s, lpRect, uFormat);
	free(s);

	return r;
}
