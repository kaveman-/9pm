#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>
#include <9pm/windows.h>

#define	WINCLASS	L"9pmgraphics"

typedef struct Window	Window;
typedef struct Winparam	Winparam;

/* mouse state */
enum {
	Minactive,	/* not active  */
	Mclick,		/* clicked on window, but have not let go */
	Mactive,	/* grabbed mosue - window is active */
};

struct Window
{
	int	id;
	int	reshaped;
	int	mstate;		/* mouse state */
	Rectangle r;
	HWND	h;
};

struct Winparam 	/* for allocating windows */
{
	int	style;
	Rectangle r;
	char	*label;
};

/* local window message */
enum {
	Walloc	= WM_USER,
	Wcursor,
};

/*
 * window interface 
 */
int	pm_wininit(int *fb, int *type, Rectangle *r, uchar [236][3]);
int	pm_winalloc(int, Rectangle*, char*);
int	pm_winreshape(int wid, Rectangle *r);
int	pm_winload(int wid, Rectangle r, int type, uchar *p, Point pt, int step);
void	cursorswitch(Cursor*);
void	cursorset(Point);

extern	HPALETTE paletteinit(uchar cmap[236][3]);
extern	void	palettecheck(HWND, HPALETTE);

HWND	libg_window;
int	libg_msg;

static void	bmiinit(void);
static void	winthread(void *);
static int	winalloc(Winparam *);
static Window	*winlookupid(int id);
static Window	*winlookuphdl(HWND h);
static Rectangle winrect(Window *w);
static HCURSOR	cursorbuild(Cursor *c);
static void	winerror();

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);


struct {
	Qlock		lk;
	int		id;
	int		nwin;
	Window		**win;
	Cursor		cursor;
	HCURSOR		hcursor;
} win;

static	Point		screenmax;
static	HINSTANCE	inst;
static	HWND		hwin;		/* hidden window */
static	HPALETTE	palette;
static  Lock		gdilock;
static 	BITMAPINFO	*bmi;
static	HCURSOR		hcursor;
static	HCURSOR		harrow;
static	int		eventfd = -1;

int
pm_wininit(int *fd, int *type, Rectangle *r, uchar cmap[256][3])
{
	int pfd[2];
	char buf[1];

	if(fd != 0) {
		if(pipe(pfd) < 0)
			fatal("wininit: could not create pipe");

		eventfd = pfd[1];
		*fd = pfd[0];
	}

	thread(winthread, cmap);

	if(read(pfd[0], buf, 1) < 1)
		fatal("wininit: bad read on event pipe");

	screenmax.x = GetSystemMetrics(SM_CXSCREEN);
	screenmax.y = GetSystemMetrics(SM_CYSCREEN);

	if(r) {
		r->min.x = r->min.y = 0;
		r->max = screenmax;
	}

	return pfd[0];
}

int
pm_winalloc(int style, Rectangle *r, char *label)
{
	int id;
	Winparam wp;

	wp.style = style;
	wp.r = *r;
	wp.label = label;

	id = SendMessage(hwin, Walloc, 0, (long)&wp);

	*r= wp.r;

	return id;
}

int
pm_winreshape(int wid, Rectangle *r)
{
	Window *w;

	qlock(&win.lk);
	
	if((w = winlookupid(wid)) == 0) {
		qunlock(&win.lk);
		return 0;
	}

	w->reshaped = 0;
	w->r = winrect(w);

	*r = w->r;
	qunlock(&win.lk);

	return 1;
}


int
pm_winload(int wid, Rectangle r, int type, uchar *p, Point pt, int step)
{
	int dx, dy, minx, i;
	Window  *w;
	HDC hdc;

	qlock(&win.lk);

	if((w = winlookupid(wid)) == 0) {
		qunlock(&win.lk);
		return 0;
	}

/*
	if(w->reshaped) {
		qunlock(&win.lk);
		return 1;
	}
*/

	minx = r.min.x & ~(31>>3);
	p += (r.min.y-pt.y)*step;
	p += minx-pt.x;
	
	if((step&3) != 0 || ((ulong)p&3) != 0)
		fatal("pm_winload: bad params %d %d %ux", step, pt.x, p);

	dx = r.max.x - r.min.x;
	dy = r.max.y - r.min.y;

	hdc = GetDC(w->h);
	SelectPalette(hdc, palette, 0);
	i = RealizePalette(hdc);

	bmi->bmiHeader.biWidth = step;
	bmi->bmiHeader.biHeight = -dy;	/* - => origin upper left */
//fprint(2, "stretchDIBits x=%d y=%d dx=%d dy=%d x2=%d\n", r.min.x-w->r.min.x, r.min.y-w->r.min.y, dx, dy, r.min.x-minx);
	StretchDIBits(hdc, r.min.x-w->r.min.x, r.min.y-w->r.min.y, dx, dy,
		r.min.x-minx, 0, dx, dy, p, bmi, DIB_PAL_COLORS, SRCCOPY);

	ReleaseDC(w->h, hdc);

	qunlock(&win.lk);

	return 1;
}

void
cursorset(Point pt)
{
	qlock(&win.lk);
	SetCursorPos(pt.x, pt.y);
	qunlock(&win.lk);
}

void
cursorswitch(Cursor *c)
{
	qlock(&win.lk);
	if(c != 0)
		win.cursor = *c;
	qunlock(&win.lk);

	if(win.nwin > 0)
		PostMessage(win.win[0]->h, Wcursor, c!=0, 0);
}

HCURSOR
cursorbuild(Cursor *c)
{
	HCURSOR nh;
	int x, y, h, w;
	uchar *sp, *cp;
	uchar *and, *xor;

	h = GetSystemMetrics(SM_CYCURSOR);
	w = (GetSystemMetrics(SM_CXCURSOR)+7)/8;

	and = mallocz(h*w);
	memset(and, 0xff, h*w);
	xor = mallocz(h*w);
	
	for(y=0,sp=c->set,cp=c->clr; y<16; y++) {
		for(x=0; x<2; x++) {
			and[y*w+x] = ~(*sp|*cp);
			xor[y*w+x] = ~*sp & *cp;
			cp++;
			sp++;
		}
	}
	nh = CreateCursor(inst, -c->offset.x, -c->offset.y,
			GetSystemMetrics(SM_CXCURSOR), h,
			and, xor);
	free(and);
	free(xor);

	return nh;
}

void
winthread(void *a)
{
/*	WNDCLASS wc; */
	WNDCLASS wc;
	MSG msg;
	uchar (*cmap)[3];
	HICON *hicon;

	cmap = a;

	inst = GetModuleHandle(NULL);
	palette = paletteinit(cmap);
	bmiinit();

	harrow = LoadCursor(NULL, IDC_ARROW);
	hicon = LoadIcon(inst, L"icon");
	if(hicon == NULL)
		hicon = LoadIcon(NULL, IDI_APPLICATION);
	
	wc.style = 0;
	wc.lpfnWndProc = WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = inst;
	wc.hIcon = hicon;
	wc.hCursor = harrow /*0*/;
	wc.hbrBackground = GetStockObject(WHITE_BRUSH);  /*GRAY_BRUSH*/
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"9pmgraphics";
	RegisterClass(&wc);

	/* create hidden window - place to send messages */
	hwin = CreateWindowEx(0,
		WINCLASS,		// window class name
		L"9pm window",		// window caption
		WS_OVERLAPPEDWINDOW,	// window style
		CW_USEDEFAULT,		// initial x position
		CW_USEDEFAULT,		// initial y position
		CW_USEDEFAULT,		// initial x size
		CW_USEDEFAULT,		// initial y size
		NULL,			// parent window handle
		NULL,			// window menu handle
		inst,			// program instance handle
		NULL);			// creation parameters

	if(hwin == 0)
		fatal("wininit");

	ShowWindow(hwin, SW_HIDE);

	write(eventfd, "w", 1);

	while(GetMessage(&msg, NULL, 0, 0)) {
		libg_msg = -2;
		TranslateMessage(&msg);
		libg_msg = -3;
		DispatchMessage(&msg);
		libg_msg = -1;
	}
	ExitProcess(0);
}

static void
bmiinit(void)
{
	ushort *p;
	int i;

	bmi = mallocz(sizeof(BITMAPINFOHEADER) + 256*sizeof(RGBQUAD));
	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biWidth = 0;
	bmi->bmiHeader.biHeight = 0;	/* - => origin upper left */
	bmi->bmiHeader.biPlanes = 1;
	bmi->bmiHeader.biBitCount = 8;
	bmi->bmiHeader.biCompression = BI_RGB;
	bmi->bmiHeader.biSizeImage = 0;
	bmi->bmiHeader.biXPelsPerMeter = 0;
	bmi->bmiHeader.biYPelsPerMeter = 0;
	bmi->bmiHeader.biClrUsed = 0;
	bmi->bmiHeader.biClrImportant = 0;	/* number of important colors: 0 means all */

	p = (ushort*)bmi->bmiColors;

	for(i = 0; i < 256; i++)
		p[i] = i;

}

static void
_kbdputc(int c)
{
	uchar buf[3];

	buf[0] = 'k';
	BPSHORT(buf+1, c);
	write(eventfd, buf, 3);
}

/* compose translation */
static void
kbdputc(int c)
{
	int i;
	static int collecting, nk;
	static Rune kc[5];

	if(c == Kalt){
		collecting = 1;
		nk = 0;
		return;
	}
	
	if(!collecting){
		_kbdputc(c);
		return;
	}
	
	kc[nk++] = c;
	c = latin1(kc, nk);
	if(c < -1)	/* need more keystrokes */
		return;
	if(c != -1)	/* valid sequence */
		_kbdputc(c);
	else
		for(i=0; i<nk; i++)
			_kbdputc(kc[i]);
	nk = 0;
	collecting = 0;
}


LRESULT CALLBACK
WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	PAINTSTRUCT paint;
	HCURSOR hcursor;
	HDC hdc;
	LONG x, y, b;
	Rectangle r;
	uchar buf[100];
	Window *w;
	int wid, i;
	int state;
	int omsg;

	omsg = libg_msg;
	libg_msg = msg;
//fprint(2, "WindowProc %ux %x %lf\n", msg, omsg, realtime());
	switch(msg) {
	case WM_CREATE:
		break;
	case WM_ACTIVATE:
		qlock(&win.lk);
		w = winlookuphdl(hwnd);
		if(w == 0) {
			qunlock(&win.lk);
			break;
		}
		switch(LOWORD(wparam)) {
		default:
			fatal("unknown WM_ACTIVE flag %d", LOWORD(wparam));
		case WA_ACTIVE:
			w->mstate = Mactive;
			break;
		case WA_CLICKACTIVE:
			w->mstate = Mclick;
			break;
		case WA_INACTIVE:
			w->mstate = Minactive;
			break;
		}
		state = w->mstate;
		qunlock(&win.lk);

		/* avoid deadlock from recursive
		 * WM_ACTIVATE messages by doing this outside the lock
		 */
		switch(state) {
		case Mactive:
		case Mclick:
			SetFocus(hwnd);
			break;
		case Minactive:
			ReleaseCapture();
			break;
		}
		SendMessage(hwnd, WM_SETCURSOR, (int)hwnd, 0);
		libg_msg = omsg;
		return 0;
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		SetCapture(hwnd);
		goto Mouse;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		ReleaseCapture();
		goto Mouse;
	case WM_MOUSEMOVE:
Mouse:
		x = (short)LOWORD(lparam);
		y = (short)HIWORD(lparam);
		b = 0;
		if(wparam & MK_LBUTTON)
			b = 1;
		if(wparam & MK_MBUTTON)
			b |= 2;
		if(wparam & MK_RBUTTON) {
			if(wparam & MK_SHIFT)
				b |= 2;
			else
				b |= 4;
		}
		
		qlock(&win.lk);
		w = winlookuphdl(hwnd);
		if(w == 0) {
			qunlock(&win.lk);
			break;
		}
		x += w->r.min.x;
		y += w->r.min.y;
		wid = w->id;
		switch(w->mstate) {
		default:
			fatal("unknown mstate");
		case Minactive:
			qunlock(&win.lk);
			libg_msg = omsg;
			return 0;
		case Mclick:
			if(b) {
				qunlock(&win.lk);
				libg_msg = omsg;
				return 0;
			}
			w->mstate= Mactive;
			break;
		case Mactive:
			break;
		}		
		qunlock(&win.lk);

		buf[0] = 'm';
		BPLONG(buf+1, w->id);
		BPLONG(buf+5, x);
		BPLONG(buf+9, y);
		BPLONG(buf+13, b);
		BPLONG(buf+17, GetTickCount());
		write(eventfd, buf, 21);
		libg_msg = omsg;
		return 0;

	case WM_CHAR:
		/* repeat count is lparam & 0xf */
		switch(wparam){
		case '\n':
			wparam = '\r';
			break;
		case '\r':
			wparam = '\n';
			break;
		}

		kbdputc(wparam);
		libg_msg = omsg;
		return 0;

	case WM_SYSKEYUP:
		libg_msg = omsg;
		return 0;

	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
		switch(wparam) {
		case VK_MENU:
			kbdputc(Kalt);
			break;
		case VK_INSERT:
			kbdputc(Kins);
			break;
		case VK_DELETE:
			kbdputc(Kdel);
			break;
		case VK_UP:
			kbdputc(Kup);
			break;
		case VK_DOWN:
			kbdputc(Kdown);
			break;
		case VK_LEFT:
			kbdputc(Kleft);
			break;
		case VK_RIGHT:
			kbdputc(Kright);
			break;
		}
		libg_msg = omsg;
		return 0;

	case WM_CLOSE:
		DestroyWindow(hwnd);
		libg_msg = omsg;
		return 0;

	case WM_DESTROY:
		(*exits)(0);
		PostQuitMessage(0);
		libg_msg = omsg;
		return 0;

	case WM_QUIT:
		(*exits)(0);
		libg_msg = omsg;
		return 0;

	case WM_PALETTECHANGED:
//fprint(2, "WM_PALETTECHANGED\n");
		if((HWND)wparam == hwnd || (HWND)wparam == hwin)
			break;
		hdc = GetDC(hwnd);
		SelectPalette(hdc, palette, 0);
		if((i = RealizePalette(hdc)) != 0)
			InvalidateRect(hwnd, 0, 0);
		ReleaseDC(hwnd, hdc);
		libg_msg = omsg;
		return 0;

	case WM_QUERYNEWPALETTE:
//fprint(2, "WM_QUERYNEWPALETTE\n");
		hdc = GetDC(hwnd);
		SelectPalette(hdc, palette, 0);
		if((i = RealizePalette(hdc)) != 0)
			InvalidateRect(hwnd, 0, 0);
		ReleaseDC(hwnd, hdc);
		libg_msg = omsg;
		return 0;

	case WM_PAINT:	
		hdc = BeginPaint(hwnd, &paint);
		r.min.x = paint.rcPaint.left;
		r.min.y = paint.rcPaint.top;
		r.max.x = paint.rcPaint.right;
		r.max.y = paint.rcPaint.bottom;
		EndPaint(hwnd, &paint);

//fprint(2, "WM_PAINT: %R\n", r);
		qlock(&win.lk);
		w = winlookuphdl(hwnd);
		if(w == 0 || w->reshaped) {
			qunlock(&win.lk);
			break;
		}
		r.min.x += w->r.min.x;
		r.min.y += w->r.min.y;
		r.max.x += w->r.min.x;
		r.max.y += w->r.min.y;
		wid = w->id;
		qunlock(&win.lk);


		buf[0] = 'r';
		BPLONG(buf+1, wid);
		BPLONG(buf+5, r.min.x);
		BPLONG(buf+9, r.min.y);
		BPLONG(buf+13, r.max.x);
		BPLONG(buf+17, r.max.y);
		write(eventfd, buf, 21);
		libg_msg = omsg;
		return 0;

	case WM_SIZE:
	case WM_MOVE:
		qlock(&win.lk);
		w = winlookuphdl(hwnd);
		if(w == 0) {
			qunlock(&win.lk);
			break;
		}
		r = winrect(w);
		w->reshaped = r.min.x != w->r.min.x ||
				r.min.y != w->r.min.y ||
				r.max.x != w->r.max.x ||
				r.max.y != w->r.max.y;
		if(w->reshaped)
			i = w->id;
		else
			i = -1;
		qunlock(&win.lk);

		if(i >= 0) {
			buf[0] = 't';
			BPLONG(buf+1, i);
			write(eventfd, buf, 5);
		}

		libg_msg = omsg;
		return 0;

	case WM_SETFOCUS:
		libg_msg = omsg;
		return 0;
	case WM_KILLFOCUS:
		libg_msg = omsg;
		return 0;

	/* 9pm specific message */
	case Walloc:
		libg_msg = omsg;
		return winalloc((Winparam*)lparam);

	case Wcursor:
		qlock(&win.lk);
		hcursor = 0;
		if(wparam == 1)
			hcursor = cursorbuild(&win.cursor);
		if(hcursor == 0)
			hcursor = harrow;
		if(SetClassLong(hwnd, GCL_HCURSOR, (long)hcursor) == 0) {	
			winerror();
			fatal("setclasslong: %r");
		}
		SetCursor(hcursor);
		if(win.hcursor && win.hcursor != harrow && !DestroyCursor(win.hcursor)) {
			winerror();
			fatal("cursor destory failed: %r\n");
		}
		win.hcursor = hcursor;
		qunlock(&win.lk);
		libg_msg = omsg;
		return 0;

	/* still to do */
	case WM_SYSCOLORCHANGE:
	case WM_COMMAND:
	case WM_DEVMODECHANGE:
	case WM_WININICHANGE:
	case WM_INITMENU:
		break;
	}
	libg_msg = omsg;
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static int
winalloc(Winparam *wp)
{
	HWND hw;
	Window *w;
	Rune label[200];

	if(wp->label == 0)
		wstrcpy(label, L"9pm window");
	else
		utftowstr(label, wp->label, sizeof(label));

	/* create hidden window - place to send messages */
	hw = CreateWindowEx(0,
		WINCLASS,		// window class name
		label,			// window caption
		WS_OVERLAPPEDWINDOW,	// window style
		CW_USEDEFAULT,		// initial x position
		CW_USEDEFAULT,		// initial y position
		CW_USEDEFAULT,		// initial x size
		CW_USEDEFAULT,		// initial y size
		NULL,			// parent window handle
		NULL,			// window menu handle
		inst,			// program instance handle
		NULL);			// creation parameters

	if(hw == 0) {
		winerror();
		return -1;
	}

	qlock(&win.lk);

	w = mallocz(sizeof(Window));
	w->id = win.id++;

	win.win = reallocz(win.win, win.nwin+1*sizeof(Window*));
	memmove(win.win+1, win.win, win.nwin*sizeof(Window*));
	win.win[0] = w;
	win.nwin++;

	w->h = hw;

	libg_window = hw;

	w->r = winrect(w);

	wp->r = w->r;

	qunlock(&win.lk);

	ShowWindow(hw, SW_SHOW);

	return w->id;
}


static Window *
winlookupid(int id)
{
	int i;

	assert(holdqlock(&win.lk));

	for (i = 0; i < win.nwin; i++)
		if (win.win[i]->id == id)
			return win.win[i];

	return 0;
}

static Window *
winlookuphdl(HWND h)
{
	int i;

	assert(holdqlock(&win.lk));

	for (i = 0; i < win.nwin; i++)
		if (win.win[i]->h == h)
			return win.win[i];

	return 0;
}

static Rectangle
winrect(Window *w)
{
	Rectangle r;
	RECT rect;
	POINT pt;
	WINDOWPLACEMENT wp;

	wp.length = sizeof(wp);
	
	if(!GetWindowPlacement(w->h, &wp))
		fatal("winrect: GetWindowPlacement");

	switch(wp.showCmd) {
	default:
		fatal("winrect: unkown showCmd: %d", wp.showCmd);
	case SW_SHOWMINIMIZED:
		r.min.x = wp.rcNormalPosition.left;
		r.min.y = wp.rcNormalPosition.top;
		r.max.x = wp.rcNormalPosition.right;
		r.max.y = wp.rcNormalPosition.bottom;
		break;

	case SW_SHOWMAXIMIZED:
	case SW_SHOWNORMAL:	
		pt.x = pt.y = 0;
		ClientToScreen(w->h, &pt);
		r.min.x = pt.x;
		r.min.y = pt.y;
		GetClientRect(w->h, &rect);
		r.max.x = r.min.x + rect.right;
		r.max.y = r.min.y + rect.bottom;
		break;
	}

	return r;
}

int
clipreadunicode(uchar *buf, int n, HANDLE h)
{
	Rune *p;
	
	p = GlobalLock(h);
	wstrtoutf(buf, p, n);
	GlobalUnlock(h);

	return strlen(buf);
}

int
clipreadutf(char *buf, int n, HANDLE h)
{
	char *p;
	int n2;

	p = GlobalLock(h);
	n2 = strlen(p);
	if(n2 < n)
		n = n2;
	memmove(buf, p, n);
	GlobalUnlock(h);
	
	return n;
}


int
clipread(uchar *buf, int n)
{
	HANDLE h;

	if(!OpenClipboard(hwin)) {
		winerror();
		return -1;
	}

	if(h = GetClipboardData(CF_UNICODETEXT))
		n = clipreadunicode(buf, n, h);
	else if(h = GetClipboardData(CF_TEXT))
		n = clipreadutf(buf, n, h);
	else {
		winerror();
		n = -1;
	}
	
	CloseClipboard();
	return n;
}

int
clipwrite(uchar *buf, int n)
{
	HANDLE h;
	char *p, *e;
	Rune *rp;

	if(!OpenClipboard(hwin)) {
		winerror();
		return -1;
	}

	if(!EmptyClipboard()) {
		winerror();
		CloseClipboard();
		return -1;
	}

	h = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, (n+1)*sizeof(Rune));
	if(h == NULL)
		fatal("out of memory");
	rp = GlobalLock(h);
	p = (char*)buf;
	e = p+n;
	while(p<e)
		p += chartorune(rp++, p);
	*rp = 0;
	GlobalUnlock(h);

	SetClipboardData(CF_UNICODETEXT, h);

	h = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, n+1);
	if(h == NULL)
		fatal("out of memory");
	p = GlobalLock(h);
	memcpy(p, buf, n);
	p[n] = 0;
	GlobalUnlock(h);
	
	SetClipboardData(CF_TEXT, h);

	CloseClipboard();
	return n;
}

static void
winerror(void)
{
	char tmp[ERRLEN];

	win_error(tmp, ERRLEN);
	errstr(tmp);
}
