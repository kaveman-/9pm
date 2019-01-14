#include <u.h>
#include <9pm/libc.h>
#include <9pm/windows.h>
#include "9pm.h"

HANDLE	fdexport(int fd);
Rune	*envblock(char**, char*);
int	execpath(Rune *path, char *file);
Rune	*proccmd(char **argv);
int	findexec(Rune *path, char *file);

static  BOOL WINAPI handler(DWORD a);

static	Qlock	proclk;
static	void	(*handlerfunc)(void);


int	consintrbug;

/* executable type */
enum {
	Econsole,
	Ercs,
};

static int	hasconsole;

void
procinit(void)
{	
	HANDLE h;

	/* determine if process has a console */
	h = CreateFile(L"CONIN$", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if(h != INVALID_HANDLE_VALUE) {
		hasconsole = 1;
		CloseHandle(h);
		SetConsoleCtrlHandler(handler, 1);
	}
}

uint
proc(char **argv, int stdin, int stdout, int stderr, char **envp)
{
	HANDLE h[3];
	char *p, *arg0, *pvar, *p2, buf[MAX_PATH], **rcargv;
	Rune path[MAX_PATH], *cmd;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	int r, i, n, flag, type, full;
	File *f;
	Rune *eb;
	char *stdpipe;

	arg0 = argv[0];
	if(arg0 == 0) {
		werrstr("null argv[0]");
		return 0;
	}

	full = arg0[0] == '\\' || arg0[0] == '/' || arg0[0] == '.';
	type = execpath(path, arg0);

	if(type < 0 && !full && (pvar = getenv("path"))) {
		for(p=pvar; p&&*p; p=p2) {
			p2 = strchr(p, ';');
			if(p2)
				*p2++ = 0;
			snprint(buf, nelem(buf), "%s/%s", p, arg0);
			type = execpath(path, buf);
			if(type >= 0)
				break;
		}
		free(pvar);
	}

	if(type < 0 && !full) {
		snprint(buf, nelem(buf), "%s/bin/%s", pm_root, arg0);
		type = execpath(path, buf);
	}

	if(type < 0 && !full)
		type = findexec(path, arg0);

	if(type < 0) {
		werrstr("file not found");
		return 0;
	}

	h[0] = fdexport(stdin);
	h[1] = fdexport(stdout);
	h[2] = fdexport(stderr);

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;
	si.wShowWindow = SW_SHOW;
	si.hStdInput = h[0];
	si.hStdOutput = h[1];
	si.hStdError = h[2];

	flag = CREATE_UNICODE_ENVIRONMENT|CREATE_SEPARATE_WOW_VDM;

	if(!hasconsole && type == Econsole) {
		/* keep the console window hidden */
		si.wShowWindow = SW_HIDE;
	}

	qlock(&proclk);

	/* hack to enable control-d */
	f = filelookup(stdin);
	if(f && f->type == Tstdin)
		stdpipe = "interactive";
	else
		stdpipe = "";

	if(envp)
		eb = envblock(envp, stdpipe);
	else {
		putenv("stdin-pipe", stdpipe);
		eb = getenvblock();
	}

	switch(type) {
	default:
		fatal("proc: bad type");
	case Econsole:
		cmd = proccmd(argv);
		break;
	case Ercs:
		for(n=0; argv[n]; n++)
			;
		wstrtoutf(buf, path, MAX_PATH);
		rcargv = malloc((n+3)*sizeof(char**));
		rcargv[0] = "rcsh";
		rcargv[1] = "-I";
		rcargv[2] = buf;
		for(i=1; i<=n; i++)
			rcargv[i+2] = argv[i];
		cmd = proccmd(rcargv);
		free(rcargv);
		snprint(buf, MAX_PATH, "%s/bin/rcsh.exe", pm_root);
		utftowstr(path, buf, MAX_PATH);
		break;
	}
	r = CreateProcess(path, cmd, 0, 0, 1, flag, eb, 0, &si, &pi);
	qunlock(&proclk);

	/* allow child to run */
	Sleep(0);

	free(cmd);
	free(eb);

	CloseHandle(h[0]);
	CloseHandle(h[1]);
	CloseHandle(h[2]);

	if(!r) {
		winerror();
		return 0;
	}

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	
	return pi.dwProcessId;
}

int
execpath(Rune *path, char *file)
{
	Rune *p;

	if(setpath(path, file) != Pfile)
		return -1;

	p = wstrrchr(path, '.');
	if(p) {
		if(wstrcmp(p, L".rcs") == 0)
		if(GetFileAttributes(path) != -1)
			return Ercs;
		if(GetFileAttributes(path) != -1)
			return Econsole;
	}
	p = path+wstrlen(path);
	wstrecpy(p, L".exe", path+MAX_PATH);
	if(GetFileAttributes(path) != -1)
		return Econsole;
	wstrecpy(p, L".rcs", path+MAX_PATH);
	if(GetFileAttributes(path) != -1)
		return Ercs;
	return -1;
}

int
findexec(Rune *path, char *file)
{
	Rune key[MAX_PATH];
	HKEY app;
	long res, n;
	int type = -1;

	if(utftowstr(key, file, MAX_PATH) >= MAX_PATH)
		return -1;

	res = RegOpenKey(HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths",
		&app);
	if(res) {
		SetLastError(res);
		winerror();
		fprint(2, "could not open APP Paths: %r\n");
		return -1;
	}
	
	n = MAX_PATH*sizeof(Rune);
	if(RegQueryValue(app, key, path, &n) == 0)
	if(GetFileAttributes(path) != -1)
		type = Econsole;

	if(type < 0) {
		wstrecpy(key+wstrlen(key), L".exe", key+MAX_PATH);
		if(RegQueryValue(app, key, path, &n) == 0)
		if(GetFileAttributes(path) != -1)
				type = Econsole;
	}
	RegCloseKey(app);
	return type;
}

Rune*
envblock(char **envp, char *stdpipe)
{
	int i, n;
	Rune *buf, *p;


	for(i=0,n=0; envp[i]; i++)
		n += utflen(envp[i])+1;
	n++;
	n += wstrlen(L"stdin-pipe=");
	n += utflen(stdpipe);
	n++;

	buf = mallocz(n*sizeof(Rune));
	for(p=buf,i=0; envp[i]; i++) {
		p += utftowstr(p, envp[i], n);
		p++;
	}
	p = wstrecpy(p, L"stdin-pipe=", buf+n);
	p += utftowstr(p, stdpipe, n);
	p++;
	*p++ = 0;
	assert(p-buf == n);

	return buf;
}

int
procwait(uint pid, double t[3])
{
	HANDLE h;
	int exit;
	FILETIME ft[4];
	vlong x;
	
	if(pid == 0)
		return 0;

	h = OpenProcess(PROCESS_ALL_ACCESS, 0, pid);
	if(h == 0) {
/*		fatal(L"race %d", pid); */
		if(t)
			t[0] = t[1] = t[2] = 0.0;
		return 0;	/* race condition - should do better */
	}

	if(WaitForSingleObject(h, INFINITE) == WAIT_FAILED) {
		winerror();
		fatal("procwait: ");
	}

	if(!GetExitCodeProcess(h, &exit)) {
		winerror();
		exit = 1;
	}

	if(t) {
		t[0] = t[1] = t[2] = 0;
		if(GetProcessTimes(h, ft, ft+1, ft+2, ft+3)) {
			x = ft[3].dwLowDateTime +
				((vlong)ft[3].dwHighDateTime<<32);
			t[0] = x*1e-7;
			x = ft[2].dwLowDateTime +
				((vlong)ft[2].dwHighDateTime<<32);
			t[1] = x*1e-7;

			x = ft[1].dwLowDateTime +
				((vlong)ft[1].dwHighDateTime<<32);
			x -= ft[0].dwLowDateTime +
				((vlong)ft[0].dwHighDateTime<<32);
			t[2] = x*1e-7;
		}
	}

	CloseHandle(h);

	return exit;
}

int
prockill(uint pid)
{
	HANDLE h;
	
	h = OpenProcess(PROCESS_ALL_ACCESS, 0, pid);
	if(h == 0)
		return 0;
	TerminateProcess(h, 1);

	CloseHandle(h);

	return 1;
}


static HANDLE
fdexport(int fd)
{
	File *f;
	HANDLE h;

	if((f = filelookup(fd)) == 0)
		return INVALID_HANDLE_VALUE;

	qlock(&f->lk);
	switch(f->type) {
	default:
		qunlock(&f->lk);
		errstr("invalid stdio fd");
		return INVALID_HANDLE_VALUE;
	case Tfile:
	case Tpipe:
	case Tstdin:
	case Tconsole:
		if(!DuplicateHandle(GetCurrentProcess(), f->h,
				GetCurrentProcess(), &h, DUPLICATE_SAME_ACCESS,
				1, DUPLICATE_SAME_ACCESS))
			h = INVALID_HANDLE_VALUE;
		break;
	}


	qunlock(&f->lk);
	return h;
}

int
consintr(void)
{
	if(!GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0)) {
		winerror();
		return 0;
	}
	/* avoid race condition with pipes and interupts on windows 95 */
	if(consintrbug)
		Sleep(300);
	return 1;
}

static BOOL WINAPI
handler(DWORD a)
{
	void	(*fnc)(void);
	
	fnc = handlerfunc;
	if(fnc != 0) {
		thread((void*)fnc, 0);
		return 1;
	}
	ExitProcess(0);
	return 0;
}

int
consonintr(void (*f)(void))
{
	handlerfunc = f;
	return 1;
}

/*
 * windows quoting rules - I think
 * Words are seperated by space or tab
 * Words containing a space or tab can be quoted using "
 * 2N backslashes + " ==> N backslashes and end quote
 * 2N+1 backslashes + " ==> N backslashes + literal "
 * N backslashes not followed by " ==> N backslashes
 */
static Rune *
dblquote(Rune *cmd, char *s)
{
	int nb, i;
	char *p;

	for(p=s; *p; p++)
		if(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '"')
			break;

	if(*p == 0) {
		/* easy case */
		while(*s)
			s += chartorune(cmd++, s);
		*cmd = 0;
		return cmd;
	}

	*cmd++ = '"';
	
	for(;;) {
		for(nb=0; *s=='\\'; s++)
			nb++;

		if(*s == 0) {
			for(i=0; i<nb; i++){
				*cmd++ = '\\';
				*cmd++ = '\\';
			}
			break;
		}

		if(*s == '"') {
			for(i=0; i<nb; i++){
				*cmd++ = '\\';
				*cmd++ = '\\';
			}
			*cmd++ = '\\';
			*cmd++ = '"';
			s++;
		} else {
			for(i=0; i<nb; i++)
				*cmd++ = '\\';
			s += chartorune(cmd++, s);
		}
	}

	*cmd++ = '"';
	*cmd = 0;

	return cmd;
}

Rune *
proccmd(char **argv)
{
	int i, n;
	Rune *cmd, *p;

	/* conservative guess at size of cmd line */
	for(i=0,n=0; argv[i]; i++) {
		n += 3 + 2*strlen(argv[i]);
	}
	n += 2;
	
	cmd = malloc(n*sizeof(Rune));
	for(i=0,p=cmd; argv[i]; i++) {
		p = dblquote(p, argv[i]);
		*p++ = ' ';
	}
	*--p = 0;

	return cmd;
}
