#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <assert.h>

#include <mp.h>
#include <libsec.h>

#include "srx.h"
#include <process.h>

static char *wsaerror(int e);

typedef struct BlThread BlThread;
typedef struct StartInfo StartInfo;

struct BlThread
{
	int tid;
	int nattach;
	char *errstr;
	char *errtype;
	char *errmsg;
};

struct StartInfo
{
	void (*f)(long);
	long a;
	long tid;
};

#define blGetThread() ((BlThread*)(TlsGetValue(tlsKey)))

int tlsKey;

static double fasttick;

void
blSleep(long msec)
{
	Sleep(msec);
}

static void
tramp(void *a)
{
	StartInfo *si = a;
	void (*f)() = si->f;
	long sa = si->a;

	blFree(si);

	blAttach();
	(*f)(sa);
	blThreadExit();
}

long
blThreadCreate(void(*f)(long), long a)
{
	StartInfo *si;

	si = blMallocZ(sizeof(si));
	si->f = f;
	si->a = a;
	if(_beginthread(tramp, 0, si) == -1)
		return 0;
	return 1;
}

void
blThreadExit(void)
{
	while(blGetThread())
		blDetach();
	_endthread();
}

double
blThreadTime(void)
{
	FILETIME t[4];
	
	if(!GetThreadTimes(GetCurrentThread(), t, t+1, t+2, t+3))
		return 0.0;
 
	return (t[2].dwLowDateTime + t[3].dwLowDateTime)*1e-7 +
		(t[2].dwHighDateTime + t[3].dwHighDateTime)*4294967296*1e-7;
}

double
blRealTime(void)
{
	LARGE_INTEGER ti;

	if(fasttick != 0 && QueryPerformanceCounter(&ti))
		return (double)(ti.QuadPart) * fasttick;

	return GetTickCount()*0.001;
}

void
blAttach(void)
{
	BlThread *t;
	static init;
	LARGE_INTEGER fi;

	if(!init) {
		if(tlsKey == 0)
			tlsKey = TlsAlloc();
		if(tlsKey == -1)
			blFatal("could not allocate thread local storage");

		// avoid zero
		if(tlsKey == 0) {
			int tt = TlsAlloc();
			if(tt == -1)
				blFatal("could not allocate thread local storage");
			assert(tt != 0);
			TlsFree(tlsKey);
			tlsKey = tt;
		}
		setvbuf(stdout, NULL, _IONBF, 0);
		setvbuf(stderr, NULL, _IONBF, 0);

		if(QueryPerformanceFrequency(&fi))
		if(fi.QuadPart > 0) {
			fasttick = 1./(double)fi.QuadPart;
// fprintf(stderr, "fasttick = %g\n", fasttick);
		}

		init = 1;
	}
	
	t = TlsGetValue(tlsKey);
	if(t != NULL) {
		assert(t->nattach > 0);
		t->nattach++;
		return;
	}
	t = blMallocZ(sizeof(BlThread));
	t->nattach = 1;
	t->tid = GetCurrentThreadId();

	TlsSetValue(tlsKey, t);
}

void
blDetach(void)
{
	BlThread *t = TlsGetValue(tlsKey);
	assert(t != NULL);
	assert(t->nattach > 0);
	t->nattach--;
	if(t->nattach > 0)
		return;
	blClearError();
	free(t);
	TlsSetValue(tlsKey, 0);
}

void
blExit(int c)
{
	exit(c);
}

void
blFree(void *p)
{
	free(p);
}


void *
blMalloc(int size)
{
	void *p;

	p = malloc(size);
	if(p == 0)
		blFatal("blMalloc: out of memory");
	return p;
}

void *
blMallocZ(int size)
{
	void *p = blMalloc(size);
	memset(p, 0, size);
	return p;
}

void *
blRealloc(void *p, int size)
{
	if(p == NULL)
		return blMalloc(size);
	p = realloc(p, size);
	if(p == 0)
		blFatal("blRealloc: out of memory");
	return p;
}

char*
blStrDup(char *s)
{
	int n;
	char *ss;

	if(s == NULL)
		return NULL;
	n = strlen(s) + 1;
	ss = blMalloc(n);
	memmove(ss, s, n);
	return ss;
}

char *
blOSError(char *prefix)
{
	int e, r;
	char buf[200], *p, *q;

	e = GetLastError();
	
	r = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
		0, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buf, sizeof(buf), 0);

	if(r == 0) {
		p = wsaerror(e);
		if(p == NULL)
			sprintf(buf, "error %d", e);
		else
			strcpy(buf, p);
	}

	for(p=q=buf; *p; p++) {
		if(*p == '\r')
			continue;
		if(*p == '\n')
			*q++ = ' ';
		else
			*q++ = *p;
	}
	*q = 0;

	if(prefix)
		blSetErrorEx("windows", prefix, buf);
	else
		blSetError("windows", buf);
	return blGetError();
}


char *
wsaerror(int e)
{
	switch(e){
	case WSAEINTR:		return "Interrupted system call.";
	case WSAEBADF:		return "Bad file number.";
	case WSAEFAULT:		return "Bad address.";
	case WSAEINVAL:		return "Invalid argument.";
	case WSAEMFILE:		return "Too many open files.";
	case WSAEWOULDBLOCK:	return "Operation would block.";
	case WSAEINPROGRESS:	return "Operation now in progress.";
	case WSAEALREADY:	return "Operation already in progress.";
	case WSAENOTSOCK:	return "Socket operation on nonsocket.";
	case WSAEDESTADDRREQ:	return "Destination address required.";
	case WSAEMSGSIZE:	return "Message too long.";
	case WSAEPROTOTYPE:	return "Protocol wrong type for socket.";
	case WSAENOPROTOOPT:	return "Protocol not available.";
	case WSAEPROTONOSUPPORT: return "Protocol not supported.";
	case WSAESOCKTNOSUPPORT: return "Socket type not supported.";
	case WSAEOPNOTSUPP:	return "Operation not supported on socket.";
	case WSAEPFNOSUPPORT:	return "Protocol family not supported.";
	case WSAEAFNOSUPPORT:	return "Address family not supported by protocol family.";
	case WSAEADDRINUSE:	return "Address already in use.";
	case WSAEADDRNOTAVAIL:	return "Cannot assign requested address.";
	case WSAENETDOWN:	return "Network is down.";
	case WSAENETUNREACH:	return "Network is unreachable.";
	case WSAENETRESET:	return "Network dropped connection on reset.";
	case WSAECONNABORTED:	return "Software caused connection abort.";
	case WSAECONNRESET:	return "Connection reset by peer.";
	case WSAENOBUFS:	return "No buffer space available.";
	case WSAEISCONN:	return "Socket is already connected.";
	case WSAENOTCONN:	return "Socket is not connected.";
	case WSAESHUTDOWN:	return "Cannot send after socket shutdown.";
	case WSAETOOMANYREFS:	return "Too many references: cannot splice.";
	case WSAETIMEDOUT:	return "Connection timed out.";
	case WSAECONNREFUSED:	return "Connection refused.";
	case WSAELOOP:		return "Too many levels of symbolic links.";
	case WSAENAMETOOLONG:	return "File name too long.";
	case WSAEHOSTDOWN:	return "Host is down.";
	case WSAEHOSTUNREACH:	return "No route to host.";
	case WSASYSNOTREADY:	return "The network subsystem is unusable.";
	case WSAVERNOTSUPPORTED: return "The Windows Sockets DLL cannot support this application.";
	case WSANOTINITIALISED:	return "Winsock not initialized.";
	case WSAEDISCON:	return "Disconnect.";
	case WSAHOST_NOT_FOUND:	return "Host not found.";
	case WSATRY_AGAIN:	return "Nonauthoritative host not found.";
	case WSANO_RECOVERY:	return "Nonrecoverable error.";
	case WSANO_DATA:	return "Valid name, no data record of requested type.";
	}
	return 0;
}

void
blFatal(char *fmt, ...)
{
	char path[MAX_PATH];
	char buf[400];
	char *p, *ep;
	va_list arg;

	path[0] = 0;
	GetModuleFileName(GetModuleHandle(0), path, sizeof(path));

	va_start(arg, fmt);
	if(GetStdHandle(STD_ERROR_HANDLE) == INVALID_HANDLE_VALUE) {
		p = buf;
		ep = p + sizeof(buf);
		p += _snprintf(p, ep-p, "%s: ", path);
		_vsnprintf(p, ep-p, fmt, arg);
		MessageBox(0, buf, "Fatal error", MB_OK);
	} else {
		fprintf(stderr, "fatal error: ");
		vfprintf(stderr, fmt, arg);
		fprintf(stderr, "\n");
		fflush(stderr);
	}
	va_end(arg);
	exit(1);
}

char *
blGetError(void)
{
	BlThread *t = blGetThread();
	if(t->errstr == 0)
		return "bowline: no error";
	return t->errstr;
}

char *
blGetErrorType(void)
{
	BlThread *t = blGetThread();
	if(t->errtype == 0)
		return "bowline";
	return t->errtype;
}

char *
blGetErrorMessage(void)
{
	BlThread *t = blGetThread();
	if(t->errmsg == 0)
		return "no error";
	return t->errmsg;
}

void
blSetError(char *type, char *msg)
{
	BlThread *t = blGetThread();

	if(type == NULL)
		type = "bowline";
	blClearError();
	t->errtype = blStrDup(type);
	t->errmsg = blStrDup(msg);
	t->errstr = blMalloc(strlen(type) + strlen(msg) + 3);
	strcpy(t->errstr, type);
	strcat(t->errstr, ": ");
	strcat(t->errstr, msg);
}

void
blSetErrorEx(char *type, char *msg, char *msg2)
{
	char *s;
	
	s = blMalloc(strlen(msg) + strlen(msg2) + 3);
	strcpy(s, msg);
	strcat(s, ": ");
	strcat(s, msg2);
	blSetError(type, s);
	blFree(s);
}

void
blClearError(void)
{
	BlThread *t = blGetThread();
	if(t->errstr == NULL)
		return;
	free(t->errstr);
	free(t->errtype);
	free(t->errmsg);
	t->errstr = NULL;
	t->errtype = NULL;
	t->errmsg = NULL;
}
