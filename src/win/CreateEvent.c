#include "win.h"

#undef CreateEvent
HANDLE WINAPI
CreateEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset,
	BOOL bInitialState, LPCWSTR lpName)
{
	char *name, buf[MAX_PATH];

	if(win_useunicode)
		return CreateEventW(lpEventAttributes, bManualReset,
			bInitialState, lpName);

	name = 0;
	if(lpName) {
		win_wstr2utf(buf, sizeof(buf), (Rune*)lpName);
		name = buf;
	}
	return CreateEventA(lpEventAttributes, bManualReset,
			bInitialState, name);
}
