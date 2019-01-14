#include "win.h"

#undef GetFullPathName
DWORD WINAPI
GetFullPathName(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer,
	LPWSTR *lpFilePart)
{
	char path[MAX_PATH], buf[MAX_PATH], *last;
	int n;

	if(win_useunicode)
		return GetFullPathNameW(lpFileName, nBufferLength, lpBuffer,
			lpFilePart);

	win_wstr2utf(path, sizeof(path), (Rune*)lpFileName);
	n = GetFullPathNameA(path, sizeof(buf), buf, &last);
	if(n == 0)
		return 0;
	n = win_utf2wstr(lpBuffer, nBufferLength, buf);
	/* this is not right but it will do */
	*lpFilePart = lpBuffer;

	return n;
}
