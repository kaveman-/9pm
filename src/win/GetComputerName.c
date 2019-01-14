#include "win.h"

#undef GetComputerName
BOOL WINAPI
GetComputerName(LPWSTR lpBuffer, LPDWORD nSize)
{
	char sysname[MAX_COMPUTERNAME_LENGTH + 1];
	int r, i;

	if(win_useunicode)
		return GetComputerNameW(lpBuffer, nSize);

	r = GetComputerNameA(sysname, nSize);
	if(!r)
		return 0;
	for(i=0; i<=(int)*nSize; i++)
		lpBuffer[i] = sysname[i];
	return 1;
}
