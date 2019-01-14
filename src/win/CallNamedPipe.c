#include "win.h"

#undef CallNamedPipe

BOOL WINAPI
CallNamedPipe(LPCWSTR lpNamedPipeName, LPVOID lpInBuffer, DWORD nInBufferSize,
	LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesRead, DWORD nTimeOut)
{
	char name[MAX_PATH];

	if(win_useunicode)
		return CallNamedPipeW(lpNamedPipeName, lpInBuffer, nInBufferSize,
			lpOutBuffer, nOutBufferSize,
			lpBytesRead, nTimeOut);

	win_wstr2utf(name, sizeof(name), (Rune*)lpNamedPipeName);
	return CallNamedPipeA(name, lpInBuffer, nInBufferSize,
			lpOutBuffer, nOutBufferSize,
			lpBytesRead, nTimeOut);
}
