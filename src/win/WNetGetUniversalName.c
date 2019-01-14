#include "win.h"

#undef WNetGetUniversalName
DWORD WINAPI
WNetGetUniversalName(LPCWSTR lpLocalPath, DWORD dwInfoLevel, LPVOID lpBuffer,
		LPDWORD lpBufferSize)
{
	if(win_useunicode)
		return WNetGetUniversalNameW(lpLocalPath, dwInfoLevel, lpBuffer,
			lpBufferSize);
	win_fatal("WNetGetUniversalName: ascii version not implemented");
}
