#include "win.h"

#undef ExtTextOut
BOOL WINAPI
ExtTextOut(HDC hdc, int x, int y, UINT opt, RECT *lprc, LPCWSTR s, int n, INT *lpDx)
{
	/* TextOutW is implemented on Windows 95 */
	return ExtTextOutW(hdc, x, y, opt, lprc, s, n, lpDx);
}

