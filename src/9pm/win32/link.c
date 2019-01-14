#include "9pm.h"
#include "link.h"

#define	 SRV	L"9pmsrv"

typedef struct Link	Link;

struct Link {
	Qlock	lk;
	Glink	*g;	/* global memory */
	HANDLE	eread;
	HANDLE	ewrite;
};
	
void	linkwatch(void *);

static Gmem	*gmem;
static HANDLE	srv;
static Link	rlink, wlink;

int
pm_linkenabled(void)
{
	return 0;
}

void
linkinit(void)
{
	HWND hwin;
	int pid;
	HANDLE hmem;
	int i;
	STARTUPINFO sinfo;
	PROCESS_INFORMATION pinfo;

	hwin = FindWindow(SRV, SRV);

	if(hwin == 0) {
		memset(&sinfo, 0, sizeof(sinfo));
		if(!CreateProcess(0, L"9pmsrv", 0, 0, 0, 0, 0, 0, &sinfo, &pinfo))
			fatal(L"Could not start 9pmsrv");
		for(i=0; hwin == 0 && i<10; i++) {
			Sleep(500);
			hwin = FindWindow(SRV, SRV);
		}
	}

	if(hwin == 0) 
		fatal(L"9pm is not running");

	GetWindowThreadProcessId(hwin, &pid);
	srv = OpenProcess(PROCESS_ALL_ACCESS, 0, pid);
	if(srv == 0)
		fatal(L"Could not get handle to 9pm");

	hmem = (void*)SendMessage(hwin, WM_USER, 0, GetCurrentProcessId());
	if(hmem == 0)
		fatal(L"Failed to get handle to 9pm shared mem");
	
	gmem = MapViewOfFile(hmem, FILE_MAP_WRITE, 0, 0, 0);
	if(gmem == 0)
		fatal(L"could not map shared memory");
	wlink.eread = gmem->u.eread;
	wlink.ewrite = gmem->u.ewrite;
	wlink.g = &gmem->u;
	rlink.eread = gmem->d.eread;
	rlink.ewrite = gmem->d.ewrite;
	rlink.g = &gmem->d;

	thread(linkwatch, srv);
}

int
pm_linkwrite(uchar *buf, int n)
{
	uchar *p;
	int ri, wi, n2;

	if(n <= 0)
		return n;

	p = buf;

	qlock(&wlink.lk);

	wi = wlink.g->wi;

	while(n>0) {
		wlink.g->writing = 1;
		for(;;) {
			if(wlink.g->closed) {
				qunlock(&wlink.lk);
				return -1;
			}
			ri = wlink.g->ri;
			ri--;
			if(ri < 0)
				ri = Gbufsize-1;
			if(wi != ri)
				break;
			if(WaitForSingleObject(wlink.ewrite, INFINITE) != WAIT_OBJECT_0)
				fatal(L"pm_linkwrite: waitforsingleobject");
		}
		wlink.g->writing = 0;

		if(wi > ri) {
			n2 = Gbufsize-wi;
			if(n2 > n)
				n2 = n;
			memmove(wlink.g->buf+wi, p, n2);
			n -= n2;
			p += n2;
			wi += n2;
			if(wi == Gbufsize)
				wi = 0;
		}

		if(n > 0 && ri != wi) {
			n2 = ri-wi;
			if(n2 > n)
				n2 = n;
			memmove(wlink.g->buf+wi, p, n2);
			n -= n2;
			p += n2;
			wi += n2;
		}

		wlink.g->wi = wi;
		if(wlink.g->reading)
			SetEvent(wlink.eread);
	}

	qunlock(&wlink.lk);

	return p-buf;
}

int
pm_linkread(uchar *buf, int n)
{
	uchar *p;
	int ri, wi, n2;

	if(n <= 0)
		return n;

	p = buf;

	qlock(&rlink.lk);
	ri = rlink.g->ri;

	rlink.g->reading = 1;
	while(ri == rlink.g->wi) {
		if(rlink.g->eof || rlink.g->closed) {
wprint(L"read link eof\n");
			qunlock(&rlink.lk);
			return -1;
		}
		if(WaitForSingleObject(rlink.eread, INFINITE) != WAIT_OBJECT_0)
			fatal(L"pm_linkread: waitforsingleobject");
	}
	rlink.g->reading = 0;

	wi = rlink.g->wi;
	if(ri > wi) {
		n2 = Gbufsize-ri;
		if(n2 > n)
			n2 = n;
		memmove(p, rlink.g->buf+ri, n2);
		n -= n2;
		p += n2;
		ri += n2;
		if(ri == Gbufsize)
			ri = 0;
	}
	if(n > 0 && ri != wi) {
		n2 = wi-ri;
		if(n2 > n)
			n2 = n;
		memmove(p, rlink.g->buf+ri, n2);
		n -= n2;
		p += n2;
		ri += n2;
	}

	rlink.g->ri = ri;
	if(rlink.g->writing)
		SetEvent(rlink.ewrite);

	qunlock(&rlink.lk);

	return p-buf;
}

void
linkwatch(void *a)
{
	HANDLE h;

	h = a;

	threadnowait();

	if(WaitForSingleObject(h, INFINITE) != WAIT_OBJECT_0)
		fatal(L"linkwatch: waitforsingleobject");
	ExitProcess(0);
}
