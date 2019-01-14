#include "win.h"

#undef GetVolumeInformation
BOOL WINAPI
GetVolumeInformation(LPCTSTR lpRootPathName, LPTSTR lpVolumeNameBuffer,
	DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber,
	LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
	LPTSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize)
{
	if(win_useunicode)
		return GetVolumeInformationW(lpRootPathName, lpVolumeNameBuffer,
			nVolumeNameSize, lpVolumeSerialNumber,
			lpMaximumComponentLength, lpFileSystemFlags,
			lpFileSystemNameBuffer, nFileSystemNameSize);
	win_fatal("GetVolumeInformation not implemented on windows 95");
}
