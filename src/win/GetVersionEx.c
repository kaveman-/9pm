#include "win.h"

#undef GetVersionEx

BOOL  WINAPI GetVersionEx(OSVERSIONINFO *lpVersionInformation)
{
	int r, i;
	OSVERSIONINFOA os;

	if(win_useunicode)
		return GetVersionExW(lpVersionInformation);

	os.dwOSVersionInfoSize = sizeof(os);
	r = GetVersionExA(&os);
	if(r == 0)
		return 0;

	lpVersionInformation->dwMajorVersion = os.dwMajorVersion;
	lpVersionInformation->dwMinorVersion = os.dwMinorVersion;
	lpVersionInformation->dwBuildNumber = os.dwBuildNumber;
	lpVersionInformation->dwPlatformId = os.dwPlatformId;	
	for(i=0; i<sizeof(os.szCSDVersion); i++)
		lpVersionInformation->szCSDVersion[i] = os.szCSDVersion[i];
	return r;
}
