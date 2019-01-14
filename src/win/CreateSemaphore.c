#include "win.h"

#undef CreateSemaphore
HANDLE WINAPI
CreateSemaphore(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount,
	LONG lMaximumCount, LPCWSTR lpName)
{
	char *name, buf[MAX_PATH];

	if(win_useunicode)
		return CreateSemaphoreW(lpSemaphoreAttributes, lInitialCount,
			lMaximumCount, lpName);

	name = 0;
	if(lpName) {
		win_wstr2utf(buf, sizeof(buf), (Rune*)lpName);
		name = buf;
	}

	return CreateSemaphoreA(lpSemaphoreAttributes, lInitialCount,
			lMaximumCount, name);
}
