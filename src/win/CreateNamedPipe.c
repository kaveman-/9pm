#include "win.h"

#undef CreateNamedPipe

HANDLE WINAPI
CreateNamedPipe(LPCWSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
	DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	if(win_useunicode)
		return CreateNamedPipeW(lpName, dwOpenMode, dwPipeMode,
			nMaxInstances, nOutBufferSize,
			nInBufferSize, nDefaultTimeOut, lpSecurityAttributes);

	win_fatal("CreateNamedPipe not supported on Windows 95");
	return 0;
}
