#include "win.h"

#pragma comment( lib, "user32.lib" )

#undef FormatMessage
DWORD WINAPI	FormatMessage(DWORD, LPCVOID, DWORD, DWORD, LPWSTR, DWORD, va_list*);

int	win_useunicode;

enum
{
	Bit1	= 7,
	Bitx	= 6,
	Bit2	= 5,
	Bit3	= 4,
	Bit4	= 3,

	T1	= ((1<<(Bit1+1))-1) ^ 0xFF,	/* 0000 0000 */
	Tx	= ((1<<(Bitx+1))-1) ^ 0xFF,	/* 1000 0000 */
	T2	= ((1<<(Bit2+1))-1) ^ 0xFF,	/* 1100 0000 */
	T3	= ((1<<(Bit3+1))-1) ^ 0xFF,	/* 1110 0000 */
	T4	= ((1<<(Bit4+1))-1) ^ 0xFF,	/* 1111 0000 */

	Rune1	= (1<<(Bit1+0*Bitx))-1,		/* 0000 0000 0111 1111 */
	Rune2	= (1<<(Bit2+1*Bitx))-1,		/* 0000 0111 1111 1111 */
	Rune3	= (1<<(Bit3+2*Bitx))-1,		/* 1111 1111 1111 1111 */

	Maskx	= (1<<Bitx)-1,			/* 0011 1111 */
	Testx	= Maskx ^ 0xFF,			/* 1100 0000 */

	Bad	= Runeerror,
};

int
win_hasunicode(void)
{
	OSVERSIONINFOA osinfo;
	int r;

	osinfo.dwOSVersionInfoSize = sizeof(osinfo);
	if(!GetVersionExA(&osinfo))
		win_fatal("GetVersionEx failed");
	switch(osinfo.dwPlatformId) {
	default:
		win_fatal("unknown PlatformId");
	case VER_PLATFORM_WIN32s:
		win_fatal("Win32s not supported");
	case VER_PLATFORM_WIN32_WINDOWS:
		r = 0;
		break;
	case VER_PLATFORM_WIN32_NT:
		r = 1;
		break;
	}

	return r;
}

char *
win_error(char *buf, int n)
{
	int e, r;
	Rune wbuf[200];
	char *p, *q;


	e = GetLastError();
	
	r = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
		0, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		wbuf, nelem(wbuf), 0);

	if(r == 0)
		wsprintfA(buf, "windows error %d", e);
	else
		win_wstr2utf(buf, n, wbuf);

	for(p=q=buf; *p; p++) {
		if(*p == '\r')
			continue;
		if(*p == '\n')
			*q++ = ' ';
		else
			*q++ = *p;
	}
	*q = 0;

	return buf;
}

int
win_chartorune(Rune *rune, char *str)
{
	int c, c1, c2;
	Rune l;

	/*
	 * one character sequence
	 *	00000-0007F => T1
	 */
	c = *(uchar*)str;
	if(c < Tx) {
		*rune = c;
		return 1;
	}

	/*
	 * two character sequence
	 *	0080-07FF => T2 Tx
	 */
	c1 = *(uchar*)(str+1) ^ Tx;
	if(c1 & Testx)
		goto bad;
	if(c < T3) {
		if(c < T2)
			goto bad;
		l = ((c << Bitx) | c1) & Rune2;
		if(l <= Rune1)
			goto bad;
		*rune = l;
		return 2;
	}

	/*
	 * three character sequence
	 *	0800-FFFF => T3 Tx Tx
	 */
	c2 = *(uchar*)(str+2) ^ Tx;
	if(c2 & Testx)
		goto bad;
	if(c < T4) {
		l = ((((c << Bitx) | c1) << Bitx) | c2) & Rune3;
		if(l <= Rune2)
			goto bad;
		*rune = l;
		return 3;
	}

	/*
	 * bad decoding
	 */
bad:
	*rune = Bad;
	return 1;
}

int
win_runetochar(char *str, Rune *rune)
{
	Rune c;

	/*
	 * one character sequence
	 *	00000-0007F => 00-7F
	 */
	c = *rune;
	if(c <= Rune1) {
		str[0] = c;
		return 1;
	}

	/*
	 * two character sequence
	 *	0080-07FF => T2 Tx
	 */
	if(c <= Rune2) {
		str[0] = T2 | (c >> 1*Bitx);
		str[1] = Tx | (c & Maskx);
		return 2;
	}

	/*
	 * three character sequence
	 *	0800-FFFF => T3 Tx Tx
	 */
	str[0] = T3 |  (c >> 2*Bitx);
	str[1] = Tx | ((c >> 1*Bitx) & Maskx);
	str[2] = Tx |  (c & Maskx);
	return 3;
}

int
win_runelen(Rune rune)
{
	char str[10];

	return win_runetochar(str, &rune);
}

int
win_fullrune(char *str, int n)
{
	int c;

	if(n > 0) {
		c = *(uchar*)str;
		if(c < Tx)
			return 1;
		if(n > 1)
			if(c < T3 || n > 2)
				return 1;
	}
	return 0;
}

long
win_utflen(char *s)
{
	int c;
	long n;
	Rune rune;

	n = 0;
	for(;;) {
		c = *(uchar*)s;
		if(c < Runeself) {
			if(c == 0)
				return n;
			s++;
		} else
			s += win_chartorune(&rune, s);
		n++;
	}
	return 0;
}

int
win_wstrutflen(Rune *s)
{
	int n;
	
	for(n=0; *s; n+=win_runelen(*s),s++)
		;
	return n;
}

int
win_wstr2utf(char *s, int n, Rune *t)
{
	int i;
	char *s0;

	s0 = s;
	if(n <= 0)
		return win_wstrutflen(t)+1;
	while(*t) {
		if(n < UTFmax+1) {
			*s = 0;
			return i+win_wstrutflen(t)+1;
		}
		i = win_runetochar(s, t);
		s += i;
		n -= i;
		t++;
	}
	*s = 0;
	return s-s0;
}

int
win_wstrlen(Rune *s)
{
	int n;

	for(n=0; *s; s++,n++)
		;
	return n;
}

int
win_utf2wstr(Rune *t, int n, char *s)
{
	int i;

	if(n <= 0)
		return win_utflen(s)+1;
	for(i=0; i<n; i++,t++) {
		if(*s == 0) {
			*t = 0;
			return i;
		}
		s += win_chartorune(t, s);
	}
	t[-1] = 0;
	return n + win_utflen(s)+1;
}


void
win_fatal(char *s)
{
	Rune buf[200];

	win_utf2wstr(buf, nelem(buf), s);
	MessageBox(0, buf, L"fatal error", MB_OK);
	ExitProcess(1);
}

void
win_convfiledata(WIN32_FIND_DATAW *fdw, WIN32_FIND_DATAA *fda)
{
	/* first fields are the same */
	memcpy(fdw, fda, (uchar*)fdw->cFileName-(char*)fdw);
	win_utf2wstr(fdw->cFileName, sizeof(fdw->cFileName), fda->cFileName);
	win_utf2wstr(fdw->cAlternateFileName, sizeof(fdw->cAlternateFileName),
			fda->cAlternateFileName);
}
