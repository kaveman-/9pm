#include "win.h"

#undef FindExecutable

HINSTANCE APIENTRY
FindExecutable(LPCWSTR lpFile, LPCWSTR lpDirectory, LPWSTR lpResult)
{
	char file[MAX_PATH], dir[MAX_PATH], exec[MAX_PATH];
	char *dirp;
	HINSTANCE res;
	int i;

	if(win_useunicode)
		return FindExecutableW(lpFile, lpDirectory, lpResult);

	win_wstr2utf(file, MAX_PATH, (Rune*)lpFile);
	if(lpDirectory) {
		dirp = dir;
		win_wstr2utf(dir, MAX_PATH, (Rune*)lpDirectory);
	} else
		dirp = 0;

	exec[0] = 0;
	res = FindExecutableA(file, dir, exec);
	
	for(i=0; exec[i]; i++)
		lpResult[i] = exec[i];
	lpResult[i] = 0;
	
	return res;	
}
