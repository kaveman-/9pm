#include <u.h>
#include <9pm/libc.h>
#include <9pm/srv.h>
#include <9pm/windows.h>

#define	WINCLASS	L"9pmsmc"

typedef struct Sid	Sid;
typedef struct Conv	Conv;
typedef struct Glink	Glink;
typedef struct Link	Link;
typedef struct Gmem	Gmem;
typedef struct Call	Call;

enum {
	Gbufsize	= 100,
};

/* sid types */
enum {
	Snet,
	Sclone,
	Sconv,	
};

enum {
	Cfree,			/* not used */
	Calloc,			/* allocated by clone - not opened */
	Cconnecting,		/* received call - not opened */
	Copened,		/* opened but not connected or announced */
	Cannounced,		/* announced - has window */
	Cannclosed,		/* a closed connection point - window is gone */
	Cconnected,		/* a connected data conversation */
};

struct Conv {
	Qlock		lk;
	int		id;
	int		state;		/* hold lk to examine or change */

	char		*port;		/* port name */

	/*
	 * parent is used to close conversation that have been allocated
 	 * but not yet opened.
	 * These conversations are closed if the parent is closed.
	 */
	Sid		*parent;

	/* for listen: state == announced */
	Event		ecall;
	HWND		wnd;		/* window for calls */
	Conv		*lis;		/* for connecting conversations */

	/* for the link - state: connecting or connected */
	int		pid;		/* process id */
	HANDLE		proc;		/* handle to processes */
	void		*gmem;		/* global memory */
	HANDLE		hgmem;		/* handle to global memory */
	Link		*rlink, *wlink;	/* read and write links */
};

struct Sid {
	int	type;
	Conv	*c;
};

struct Link {
	Qlock	lk;
	Glink	*g;	/* global memory */
	HANDLE	eread;
	HANDLE	ewrite;
};

struct Glink {
	HANDLE	eread;		/* client handle for read event object */
	HANDLE	ewrite;		/* client handle for write event object */
	int	closed;		/* read side closed */
	int	eof;		/* write side closed */
	int	reading;	/* read in progress */
	int	writing;	/* write in progress */
	int	ri;		/* only changed by reader */
	int	wi;		/* only changed by writer */
	uchar	buf[Gbufsize];	/* data */
};


struct Gmem {
	Glink	u;	/* to server */
	Glink	d;	/* to client */
};

static void	*sinit(void*, Sinfo*);
static int	sfini(void*);
static Sid	*sopen(void*, int, char*, char*, Sinfo*);
static int	sclose(void*, Sid*);
static int	sfree(void*, Sid*);
static int	sread(void*, Sid*, uchar*, int);
static int	swrite(void*, Sid*, uchar*, int);
static int	srpc(void*, Sid*, uchar*, int, int);

Stype srvsmc = {
	sinit,
	sfini,
	sopen,
	sclose,
	sfree,
	sread,
	swrite,
	srpc,
	0,
};

static int	convcall(Conv *c, uint pid);
static Conv	*convalloc(void);
static void	convclose(Conv *);
static int	convannounce(Conv *c, char *port);
static int	convlisten(Conv *c);
static int	convconnect(Conv *c, char *port);
static Link*	linkalloc(Glink*, HANDLE, HANDLE, HANDLE);
static void	linkclose(Link*, int);
static void	linkfree(Link*);
static int	linkread(Link*, uchar*, int);
static int	linkwrite(Link*, uchar*, int);
static Sid	*sidalloc(int type, Conv *c);
static void	announcethread(Conv *c);

static struct {
	Qlock	lk;
	
	int	nconv;
	Conv	**conv;
} smc;

static void *
sinit(void *a, Sinfo *info)
{

	strcpy(info->name, "/");
	strcpy(info->type, "net");
	info->flags |= Sdir;
	srvgenid(info->id, &srvsmc);
	info->mesgsize = 0;
	
	return &srvsmc;
}

static int
sfini(void *a)
{
/* fprint(2, "srvping: fini\n"); /**/
	return 1;
}

static Sid *
sopen(void *a, int id, char *name, char *type, Sinfo *info)
{
	Sid *sid;
	char *p;
	int cid;
	Conv *c;

	if(strcmp(name, "/") == 0) {
		if(strcmp(type, "net") != 0) {
			werrstr("server type: expected net");
			return 0;
		}
		sid = sidalloc(Snet, 0);
		return sid;
	}

	
	if(strcmp(name, "/clone") == 0) {
		if(strcmp(type, "netclone") != 0) {
			werrstr("server type: expected netclone");
			return 0;
		}
		sid = sidalloc(Sclone, 0);
		return sid;
	}

	if(strcmp(type, "netconv") != 0) {
		werrstr("server type: expected netconv");
		return 0;
	}

	cid = strtol(name+1, &p, 10);
	if(p == name+1 || *p != 0) {
		werrstr("unknown server");
		return 0;
	}

	qlock(&smc.lk);
	if(cid < 0 || cid >= smc.nconv) {
		qunlock(&smc.lk);
		werrstr("conversation number out of range");
		return 0;
	}
	c = smc.conv[cid];
	qunlock(&smc.lk);

	qlock(&c->lk);
	if(c->state == Cfree) {
		qunlock(&c->lk);
		werrstr("conversation not allocated");
		return 0;
	}
	if(c->state != Calloc && c->state != Cconnecting) {
		qunlock(&c->lk);
		werrstr("conversation already opened");
		return 0;
	}
	if(c->state == Cconnecting) {
		c->lis = 0;
		c->state = Cconnected;
	} else {
		c->parent = 0;
		c->state = Copened;
	}
	qunlock(&c->lk);

	sid = sidalloc(Sconv, c);
	return sid;
}

static int
sclose(void *a, Sid *sid)
{
	Conv *c;

	switch(sid->type) {
	default:
		break;
	case Sconv:
		c = sid->c;
		qlock(&c->lk);
		switch(c->state) {
		case Cconnected:
			/* close links - should wakeup any reads/writes */
			linkclose(c->rlink, 0);
			linkclose(c->wlink, 1);
			break;
		case Cannounced:
			if(c->wnd)
				PostMessage(c->wnd, WM_CLOSE, 0, 0);
			else
				c->state = Cannclosed;
			break;
		}
		qunlock(&c->lk);
	}
	
	return 1;
}

static int
sfree(void *a, Sid *sid)
{
	int i;
	Conv *c;

	switch(sid->type) {
	default:
		break;
	case Sclone:
		qlock(&smc.lk);
		for(i=0; i<smc.nconv; i++)
			if(smc.conv[i]->parent == sid)
				convclose(smc.conv[i]);
		qunlock(&smc.lk);
		break;
	case Sconv:
		c = sid->c;
		qlock(&smc.lk);
		convclose(c);
		for(i=0; i<smc.nconv; i++)
			if(smc.conv[i]->lis == c)
				convclose(c);
		qunlock(&smc.lk);
		break;
	}
	free(sid);
	return 1;
}

static int
sread(void *a, Sid *sid, uchar *buf, int n)
{
	Conv *c;

	switch(sid->type) {
	default:
		werrstr("read operation not supported");
		return -1;
	case Sclone:
		c = convalloc();
		c->parent = sid;
		n = snprint(buf, n, "%d", c->id);
		qunlock(&c->lk);
		break;
	case Sconv:
		c = sid->c;
		if(c->state != Cconnected) {
			werrstr("conversation not connected");
			return -1;
		}
		n = linkread(c->rlink, buf, n);
		break;
	}
	return n;
}

static int
swrite(void *a, Sid *sid, uchar *buf, int n)
{
	Conv *c;

	switch(sid->type) {
	default:
		werrstr("write operation not supported");
		return -1;
	case Sconv:
		c = sid->c;
		if(c->state != Cconnected) {
			werrstr("conversation not connected");
			return -1;
		}
		n = linkwrite(c->wlink, buf, n);
		break;
	}

	return n;
}

static int
srpc(void *a, Sid *sid, uchar *ubuf, int nr, int nw)
{
	Conv *c;
	int id;
	char buf[Snamelen];

	switch(sid->type) {
	default:
		werrstr("rpc operation not supported");
		return -1;
	case Sconv:
		c = sid->c;
		
		if(nr >= sizeof(buf)) {
			werrstr("rpc too long");
			return -1;
		}
		memmove(buf, ubuf, nr);
		buf[nr] = 0;

		if(strncmp(buf, "announce ", 9) == 0) {
			if(!convannounce(c, buf+9))
				return -1;
			nw = 0;
			break;
		}

		if(strncmp(buf, "connect ", 8) == 0) {
			if(!convconnect(c, buf+8))
				return -1;
			nw = 0;
			break;
		}
		if(strcmp(buf, "listen") == 0) {
			id = convlisten(c);
			if(id<0)
				return -1;
			nw = snprint(ubuf, nw, "%d", id);
			break;
		}

		if(strcmp(buf, "accept") == 0) {
			nw = 0;
			break;
		}

		if(strncmp(buf, "reject ", 7) == 0) {
			nw = 0;
			break;
		}
		werrstr("unknown rpc command");
		return -1;
		break;
	}
	return nw;
}

static Conv *
convalloc(void)
{
	Conv *c;
	int i;

	qlock(&smc.lk);
	for(i=0; i<smc.nconv; i++)
		if(smc.conv[i]->state == Cfree) 
			break;

	if(i == smc.nconv) {
		smc.nconv++;
		smc.conv = realloc(smc.conv, smc.nconv*sizeof(Conv*));
		c = mallocz(sizeof(Conv));
		smc.conv[i] = c;
		c->id = i;
	} else
		c = smc.conv[i];

	qlock(&c->lk);
	c->state = Calloc;
	qunlock(&smc.lk);
	return c;
}


static void
convclose(Conv *c)
{
	qlock(&c->lk);
	switch(c->state) {
	default:
		fatal("convclose: unknown state");
	case Cfree:
		fatal("convclose: c->state == Cfree");
	case Calloc:
	case Cconnecting:
	case Copened:
	case Cconnected:
	case Cannclosed:
		break;
	case Cannounced:
		/* announcethread will change the state to Cannclosed */
		while(c->state != Cannclosed) {
			qunlock(&c->lk);
			ewait(&c->ecall);
			qlock(&c->lk);
		}
	}

	/* general cleanup */
	free(c->port);
	c->port = 0;
	c->parent = 0;

	ereset(&c->ecall);
	c->wnd = 0;
	c->lis = 0;

	c->pid = 0;

	if(c->proc)
		CloseHandle(c->proc);
	c->proc = 0;

	if(c->gmem)
		UnmapViewOfFile(c->gmem);
	c->gmem = 0;

	if(c->hgmem)
		CloseHandle(c->hgmem);
	c->hgmem = 0;
	
	linkfree(c->rlink);
	c->rlink = 0;
	linkfree(c->wlink);
	c->wlink = 0;

	/* ok, ready to realloc it */
	c->state = Cfree;

	qunlock(&c->lk);
}

static int
convannounce(Conv *c, char *port)
{
		
	qlock(&c->lk);
	if(c->state != Copened) {
		qunlock(&c->lk);
		werrstr("conversation in use");
		return 0;
	}
	c->state = Cannounced;
	c->port = strdup(port);
	qunlock(&c->lk);
	thread(announcethread, c);
	return 1;
}

static int
convconnect(Conv *c, char *port)
{
	HWND hwin;
	int pid;
	char buf[Snamelen+30];
	Rune wbuf[Snamelen+30];
	Gmem *gm;

	qlock(&c->lk);

	if(c->state != Copened) {
		werrstr("conversation is busy");
		goto Err;
	}

	sprint(buf, "9pmsmc %s", port);
	utftowstr(wbuf, buf, sizeof(wbuf));

	hwin = FindWindow(WINCLASS, wbuf);

	if(hwin == 0) {
		werrstr("port not found");
		goto Err;
	}

	GetWindowThreadProcessId(hwin, &pid);
	CloseHandle(hwin);
	c->pid = pid;
	c->proc = OpenProcess(PROCESS_ALL_ACCESS, 0, pid);
	if(c->proc == 0)
		goto Err;

	c->hgmem = (void*)SendMessage(hwin, WM_USER, 0, GetCurrentProcessId());
	if(c->hgmem == 0)
		goto Err;
	
	gm = MapViewOfFile(c->hgmem, FILE_MAP_WRITE, 0, 0, 0);
	if(gm == 0)
		fatal("could not map shared memory");
	c->gmem = gm;
	c->rlink = linkalloc(&gm->d, 0, gm->d.eread, gm->d.ewrite);
	c->wlink = linkalloc(&gm->u, 0, gm->u.eread, gm->u.ewrite);

	c->port = strdup(port);

	c->state = Cconnected;

	qunlock(&c->lk);

	return 1;

Err:
	if(c->gmem)
		UnmapViewOfFile(c->gmem);
	c->gmem = 0;

	if(c->hgmem)
		CloseHandle(c->hgmem);
	c->hgmem = 0;
	
	c->pid = 0;
	if(c->proc)
		CloseHandle(c->proc);
	c->proc = 0;

	qunlock(&c->lk);
	return 0;
}

int
convlisten(Conv *c)
{
	int i;

	for(;;) {
		qlock(&smc.lk);
		for(i=0; i<smc.nconv; i++) {
			if(smc.conv[i]->lis == c) {
				qunlock(&smc.lk);
				return i;
			}
		}
		qunlock(&smc.lk);

		if(!ewait(&c->ecall)) {
			werrstr("listen closed");
			return -1;
		}
	}
}

static LRESULT CALLBACK 
WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;
	Conv *c;

	switch (message) {
	case WM_USER:
		c = (Conv*)GetWindowLong(hwnd, GWL_USERDATA);
		assert(c->state == Cannounced);
		return convcall(c, lParam);

	case WM_PAINT:
	       hdc = BeginPaint (hwnd, &ps);
   	       EndPaint (hwnd, &ps);
               return 0;

	case WM_DESTROY:
               PostQuitMessage (0);
               return 0;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

static void
announcethread(Conv *c)
{
	HWND hwnd;
	MSG msg;
	WNDCLASS wndclass;
	char buf[Snamelen+30], err[ERRLEN];
	Rune wbuf[Snamelen+30];
	static Ref init;

	threadnowait();

	if(refinc(&init) == 0) {
		wndclass.style         = 0;
		wndclass.lpfnWndProc   = WndProc;
		wndclass.cbClsExtra    = 0;
		wndclass.cbWndExtra    = 0;
		wndclass.hInstance     = GetModuleHandle(NULL);
		wndclass.hIcon         = LoadIcon (NULL, IDI_APPLICATION);
		wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW);
		wndclass.hbrBackground = GetStockObject (WHITE_BRUSH);
		wndclass.lpszMenuName  = NULL;
		wndclass.lpszClassName = WINCLASS;

		if(RegisterClass(&wndclass) == 0)
			fatal("RegisterClass");
	}

	qlock(&c->lk);

	if(c->state != Cannounced) {
		qunlock(&c->lk);
		threadexit();
	}

	sprint(buf, "9pmsmc %s", c->port);
	utftowstr(wbuf, buf, sizeof(wbuf));

 	hwnd = CreateWindow (WINCLASS,	// window class name
		wbuf,			// window caption
		WS_OVERLAPPEDWINDOW,	// window style
		CW_USEDEFAULT,		// initial x position
		CW_USEDEFAULT,		// initial y position
		CW_USEDEFAULT,		// initial x size
		CW_USEDEFAULT,		// initial y size
		NULL,			// parent window handle
		NULL,			// window menu handle
		GetModuleHandle(NULL),	// program instance handle
		NULL);			// creation parameters

	if(hwnd == 0)
		fatal("CreateWindow: %s", win_error(err, sizeof(err)));

	SetLastError(0);
	if(SetWindowLong(hwnd, GWL_USERDATA, (long)c) == 0 && GetLastError() != 0)
		fatal("SetWindowLong: %s", win_error(err, sizeof(err)));
	c->wnd = hwnd;

	qunlock(&c->lk);

	ShowWindow(hwnd, SW_HIDE);

	while(GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage (&msg) ;
		DispatchMessage (&msg) ;
	}

	/* cleanup closing */
	qlock(&c->lk);
	c->state = Cannclosed;
	qunlock(&c->lk);

	epoison(&c->ecall);
	
	threadexit();
}

static int
convcall(Conv *c, uint pid)
{
	HANDLE hp, hmem[2], ue[2], de[2];
	int n, i;
	Gmem *gmem;
	Conv *nc;
	char err[ERRLEN];

	hp = 0;
	hmem[0] = hmem[1] = 0;
	ue[0] = ue[1] = de[0] = de[1] = 0;
	gmem = 0;

	if((hp = OpenProcess(PROCESS_ALL_ACCESS, 0, pid)) == 0)
		goto Err;

	n = sizeof(Gmem);
	hmem[0] = CreateFileMapping((HANDLE)0xFFFFFFFF, 0, PAGE_READWRITE|SEC_COMMIT, 0, n, 0);
	if(hmem[0] == 0)
		goto Err;
  	if (DuplicateHandle(GetCurrentProcess(), hmem[0],
			hp, &hmem[1], FILE_MAP_ALL_ACCESS, 0, 0) == 0)
		goto Err;

	gmem = MapViewOfFile(hmem[0], FILE_MAP_WRITE, 0, 0, n);
	if(gmem == 0)
		goto Err;
	memset(gmem, 0, sizeof(Gmem));

	for(i=0; i<2; i++) {
		if((ue[i] = CreateEvent(0, 0, 0, 0)) == 0)
			goto Err;
		if((de[i] = CreateEvent(0, 0, 0, 0)) == 0)
			goto Err;
	}

	nc = convalloc();

	nc->port = strdup(c->port);
	nc->state = Cconnecting;
	nc->proc = hp;
	nc->lis = c;
	nc->pid = pid;
	nc->gmem = gmem;
	nc->hgmem = hmem[0];
	nc->rlink = linkalloc(&gmem->u, hp, ue[0], ue[1]);
	nc->wlink = linkalloc(&gmem->d, hp, de[0], de[1]);

	qunlock(&nc->lk);

	/* wakeup listener */
	esignal(&c->ecall);

	return (long)hmem[1];

Err:
fprint(2, "convcall: error: %s", win_error(err, sizeof(err)));
	if(hp)
		CloseHandle(hp);
	if(gmem)
		UnmapViewOfFile(gmem);
	if(hmem[0])
		CloseHandle(hmem[0]);
	for(i=0; i<2; i++) {
		if(ue[i])
			CloseHandle(ue[i]);
		if(de[i])
			CloseHandle(de[i]);
	}
	return 0;
}

static Sid *
sidalloc(int type, Conv *c)
{
	Sid *s;

	s = mallocz(sizeof(Sid));
	s->type = type;
	s->c = c;
	return s;
}

static Link*
linkalloc(Glink *gl, HANDLE hp, HANDLE eread, HANDLE ewrite)
{
	Link *l;

	if(hp) {
		DuplicateHandle(GetCurrentProcess(), eread, hp,
				&gl->eread, EVENT_ALL_ACCESS, 0, 0);
		DuplicateHandle(GetCurrentProcess(), ewrite, hp,
				&gl->ewrite, EVENT_ALL_ACCESS, 0, 0);
	}
	l = mallocz(sizeof(Link));
	l->g = gl;
	l->eread = eread;
	l->ewrite = ewrite;
	return l;
}

static int
linkwrite(Link *link, uchar *buf, int n)
{
	uchar *p;
	int ri, wi, n2;
	Glink *g;

	if(n <= 0)
		return n;

	p = buf;

	qlock(&link->lk);
	g = link->g;

	wi = g->wi;

	while(n>0) {
		g->writing = 1;
		for(;;) {
			if(g->closed) {
				qunlock(&link->lk);
				return -1;
			}
			ri = g->ri;
			ri--;
			if(ri < 0)
				ri = Gbufsize-1;
			if(wi != ri)
				break;
			if(WaitForSingleObject(link->ewrite, INFINITE) != WAIT_OBJECT_0)
				fatal("linkwrite: waitforsingleobject");
		}
		g->writing = 0;

		if(wi > ri) {
			n2 = Gbufsize-wi;
			if(n2 > n)
				n2 = n;
			memmove(g->buf+wi, p, n2);
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
			memmove(g->buf+wi, p, n2);
			n -= n2;
			p += n2;
			wi += n2;
		}

		g->wi = wi;
		if(g->reading)
			SetEvent(link->eread);
	}

	qunlock(&link->lk);

	return p-buf;
}

static int
linkread(Link *link, uchar *buf, int n)
{
	uchar *p;
	int ri, wi, n2;
	Glink *g;

	if(n <= 0)
		return n;

	p = buf;

	qlock(&link->lk);

	g = link->g;
	ri = g->ri;

	g->reading = 1;
	while(ri == g->wi) {
		if(g->eof) {
/* fprint(2, "read link eof\n"); */
			qunlock(&link->lk);
			return 0;
		}
		if(g->closed) {
			qunlock(&link->lk);
			return -1;
		}
		if(WaitForSingleObject(link->eread, INFINITE) != WAIT_OBJECT_0)
			fatal("linkread: waitforsingleobject");
	}
	g->reading = 0;

	wi = g->wi;
	if(ri > wi) {
		n2 = Gbufsize-ri;
		if(n2 > n)
			n2 = n;
		memmove(p, g->buf+ri, n2);
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
		memmove(p, g->buf+ri, n2);
		n -= n2;
		p += n2;
		ri += n2;
	}

	g->ri = ri;
	if(g->writing)
		SetEvent(link->ewrite);

	qunlock(&link->lk);

	return p-buf;
}

static void
linkclose(Link *l, int dir)
{
	if(dir == 0)
		l->g->closed = 1;
	else
		l->g->eof = 1;
	SetEvent(l->eread);
	SetEvent(l->ewrite);
}

static void
linkfree(Link *l)
{
	if(l == 0)
		return;

	CloseHandle(l->eread);
	CloseHandle(l->ewrite);
	free(l);
}

