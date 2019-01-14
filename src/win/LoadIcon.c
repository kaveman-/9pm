#include "win.h"


#undef LoadIcon
HICON WINAPI
LoadIcon(HINSTANCE hInstance, LPCWSTR lpIconName)
{
	char name[MAX_PATH];

	if(win_useunicode)
		return LoadIconW(hInstance, lpIconName);

	if((int)lpIconName < (1<<16)) {
		/* probably not a string */
		return LoadIconA(hInstance, (char*)lpIconName);
	}
	
	win_wstr2utf(name, sizeof(name), (Rune*)lpIconName);
	return LoadIconA(hInstance, name);
}
