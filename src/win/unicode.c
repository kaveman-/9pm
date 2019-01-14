
#include "win.h"

VOID WINAPI
OutputDebugString(LPCWSTR lpOutputString)
{
	char *buf;
	int n;

	if(win_useunicode) {
		OutputDebugStringW(lpOutputString);
		return;
	}

	n = wstrlen((Rune*)lpOutputString)*UTFmax+1;
	buf = malloc(n);
	win_wstr2utf(buf, n, (Rune*)lpOutputString);
	OutputDebugStringA(buf);
	free(buf);
}

	







