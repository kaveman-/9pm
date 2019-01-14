#include "win.h"

#undef CreateProcess
BOOL WINAPI
CreateProcess(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
	LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags,
	LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo,
	LPPROCESS_INFORMATION lpProcessInformation)
{
	char *app, appbuf[MAX_PATH], *cmd, *cd, cdbuf[MAX_PATH];
	char desktop[MAX_PATH], title[MAX_PATH];
	Rune *rp;
	char *env, *p;
	STARTUPINFOA si;
	int n;
	BOOL b;

	if(win_useunicode)
		return CreateProcessW(lpApplicationName, lpCommandLine, lpProcessAttributes,
			lpThreadAttributes, bInheritHandles, dwCreationFlags,
			lpEnvironment, lpCurrentDirectory, lpStartupInfo,
			lpProcessInformation);

	app = 0;
	if(lpApplicationName) {
		win_wstr2utf(appbuf, sizeof(appbuf), (Rune*)lpApplicationName);
		app = appbuf;
	}

	cmd = 0;
	if(lpApplicationName) {
		n = UTFmax*win_wstrlen(lpCommandLine);
		cmd = malloc(n);
		win_wstr2utf(cmd, n, (Rune*)lpCommandLine);
	}
	cd = 0;
	if(lpCurrentDirectory) {
		win_wstr2utf(cdbuf, sizeof(cdbuf), (Rune*)lpCurrentDirectory);
		cd = cdbuf;
	}

	memcpy(&si, lpStartupInfo, sizeof(si));
	if(lpStartupInfo->lpDesktop) {
		win_wstr2utf(desktop, sizeof(desktop), lpStartupInfo->lpDesktop);
		si.lpDesktop = desktop;
	}
	if(lpStartupInfo->lpTitle) {
		win_wstr2utf(title, sizeof(title), lpStartupInfo->lpTitle);
		si.lpTitle = title;
	}
	if(lpEnvironment) {
		for(rp=lpEnvironment,n=0; !(rp[0]==0 && rp[1]==0); rp++)
			n += win_runelen(*rp);
		n+=2;
		env = malloc(n);
		for(rp=lpEnvironment,p=env; !(rp[0]==0 && rp[1]==0); rp++)
			p += win_runetochar(p, rp);
		*p++ = 0;
		*p++ = 0;
	} else
		env = 0;

	b = CreateProcessA(app, cmd, lpProcessAttributes,
			lpThreadAttributes, bInheritHandles, dwCreationFlags,
			env, cd, &si,
			lpProcessInformation);
	free(env);
	free(cmd);
	return b;
}
