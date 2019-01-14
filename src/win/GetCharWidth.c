#include "win.h"

#undef GetCharWidth

/* implement on windows 95 */

BOOL  WINAPI
GetCharWidth(HDC hdc, UINT c0, UINT c1, INT *w)
{
	return GetCharWidthW(hdc, c0, c1, w);
}
