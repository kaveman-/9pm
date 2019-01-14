#include <u.h>
#include <9pm/libc.h>
#include <9pm/windows.h>
#include "9pm.h"

int _fltused = 0x9875;

char	*pm_root;
char	*argv0;

void	envinit(void);
int	args(char *argv[], int n, char *p);
void	platforminit(void);

BOOL WINAPI
DllMain(HANDLE hModule, DWORD type, LPVOID arg)
{
	switch(type) {
	case DLL_PROCESS_ATTACH:
		platforminit();
		threadinit();
		procinit();
		envinit();
		fdinit();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

void
pm_argv0(char *s)
{
	argv0 = s;
};


void *
sbrk(ulong n)
{
	HGLOBAL h;
	void *p;

	h = GlobalAlloc(GMEM_FIXED, n+8);
	if(h == 0)
		return (void*)-1;
/* print("sbrk %d\n", n); */

	p = GlobalLock(h);

	/* make sure it is Aligned to 8 bytes */
	p = (void*)(((ulong)p + 7) & ~7);
	memset(p, 0, n);

	return p;
}

void
szero(void *p, long nb)
{
	memset(p, 0, nb);
}


void
__exits(char *s, int debug)
{
	Rune buf[200];
	File *f;


	if(s && s[0] != 0) {
		f = filelookup(2);
		if(tmpconsole || f->type == Tfree || f->type == Tdevnull) {
			utftowstr(buf, s, nelem(buf));
			MessageBox(0, buf, L"fatal error", MB_OK);
		}
		if(debug) {
			*(char*)0 = 0;
			DebugBreak();
		}
		ExitProcess(1);
	}
	if(debug) {
		*(char*)0 = 0;
		DebugBreak();
	}

	ExitProcess(0);
}


void
_exits(char *s)
{
	__exits(s, 0);
}

double
realtime(void)
{
	return GetTickCount()*0.001;
}

double
cputime(void)
{
	FILETIME t[4];
	
	if(!GetProcessTimes(GetCurrentProcess(), t, t+1, t+2, t+3))
		return 0.0;
 
	return (t[2].dwLowDateTime + t[3].dwLowDateTime)*1e-7 +
		(t[2].dwHighDateTime + t[3].dwHighDateTime)*4294967296*1e-7;
}

double
usertime(void)
{
	FILETIME t[4];
	
	if(!GetProcessTimes(GetCurrentProcess(), t, t+1, t+2, t+3))
		return 0.0;
 
	return (t[2].dwLowDateTime)*1e-7 +
		(t[2].dwHighDateTime)*4294967296*1e-7;
}

int
sleep(long ms)
{
	Sleep(ms);
	return 0;
}

ulong
getcallerpc(void *p)
{
	ulong *lp;

	lp = p;

	return lp[-1];
}
int
PM_assert(int a, char *file, int line)
{
	int pc, n;
	char buf[300];

	pc = getcallerpc(&a);

	n = sprint(buf, "%s: assert failed at file=%s line=%d pc=%ux\n", argv0,
			file, line, pc);
	write(2, buf, n);
	__exits(buf, 1);

	return 0;	/* never gets here */
}

void
abort(void)
{
	fprint(2, "abort\n");
	__exits("abort", 1);
}

uint
getpid(void)
{
	return GetCurrentProcessId();
}

char*
getuser(void)
{
	return "none";
}

void
platforminit(void)
{
	OSVERSIONINFOA osinfo;

	osinfo.dwOSVersionInfoSize = sizeof(osinfo);
	if(!GetVersionExA(&osinfo))
		fatal("GetVersionEx failed");
	switch(osinfo.dwPlatformId) {
	default:
		fatal("unknown PlatformId: %d", osinfo.dwPlatformId);
	case VER_PLATFORM_WIN32s:
		fatal("9pm does not support Win32s");
	case VER_PLATFORM_WIN32_WINDOWS:
		win_useunicode = 0;
		usesecurity = 0;
		consintrbug = 1;
		break;
	case VER_PLATFORM_WIN32_NT:
		win_useunicode = 1;
		usesecurity = 1;
		consintrbug = 0;
		break;
	}
}
