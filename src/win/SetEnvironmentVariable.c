#include "win.h"

#undef SetEnvironmentVariable
BOOL WINAPI
SetEnvironmentVariable(LPCWSTR lpName, LPCWSTR lpValue)
{
	BOOL b;
	char name[MAX_PATH], *val;
	int n;

	if(win_useunicode)
		return SetEnvironmentVariableW(lpName, lpValue);

	win_wstr2utf(name, sizeof(name), (Rune*)lpName);
	n = win_wstrlen((Rune*)lpValue)*UTFmax;
	val = malloc(n);
	win_wstr2utf(val, n, (Rune*)lpValue);
	b = SetEnvironmentVariableA(name, val);
	free(val);

	return b;
}
