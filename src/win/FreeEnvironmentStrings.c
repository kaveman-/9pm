#include "win.h"

#undef FreeEnvironmentStrings
BOOL WINAPI
FreeEnvironmentStrings(LPWSTR s)
{
	if(win_useunicode)
		return FreeEnvironmentStringsW(s);

	free(s);
	return 1;
}
