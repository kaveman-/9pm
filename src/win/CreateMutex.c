#include "win.h"

#undef CreateMutex
HANDLE WINAPI
CreateMutex(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bInitialOwner,
	LPCWSTR lpName)
{
	char *name, buf[MAX_PATH];

	if(win_useunicode)
		return CreateMutexW(lpEventAttributes, bInitialOwner, lpName);

	name = 0;
	if(lpName) {
		win_wstr2utf(buf, sizeof(buf), (Rune*)lpName);
		name = buf;
	}
	return CreateMutexA(lpEventAttributes, bInitialOwner, name);
}
