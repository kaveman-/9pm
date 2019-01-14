#include "win.h"

#undef WNetEnumResource
DWORD WINAPI
WNetEnumResource(HANDLE  hEnum, LPDWORD lpcCount, LPVOID lpBuffer,
	LPDWORD lpBufferSize)
{
	uchar *buf;
	Rune *p;
	NETRESOURCEW *rw;
	NETRESOURCEA *ra;
	int n, n2, r, i;

	if(win_useunicode)
		return WNetEnumResourceW(hEnum, lpcCount, lpBuffer,
			lpBufferSize);

	n = *lpBufferSize;
	n /= 2;		/* be conservative */
	buf = malloc(n);
	r = WNetEnumResourceA(hEnum, lpcCount, buf, &n2);
	if(r != NO_ERROR) {
		*lpBufferSize = n2*2;
		free(buf);
		return r;
	}
	n = *lpcCount;
	ra = (NETRESOURCEA*)buf;
	rw = (NETRESOURCEW*)lpBuffer;
	p = (Rune*)(rw+n);
	for(i=0; i<n; i++,ra++,rw++) {
		memset(rw, 0, sizeof(NETRESOURCEW));
		rw->dwScope = ra->dwScope;
		rw->dwType = ra->dwType;
		rw->dwDisplayType = ra->dwDisplayType;
		rw->dwUsage = ra->dwUsage;
		if(ra->lpLocalName) {
			rw->lpLocalName = p;
			p += win_utf2wstr(p, MAX_PATH, ra->lpLocalName) + 1;
		}
		if(ra->lpRemoteName) {
			rw->lpRemoteName = p;
			p += win_utf2wstr(p, MAX_PATH, ra->lpRemoteName) + 1;
		}
		if(ra->lpComment) {
			rw->lpComment = p;
			p += win_utf2wstr(p, MAX_PATH, ra->lpComment) + 1;
		}
		if(ra->lpProvider) {
			rw->lpProvider = p;
			p += win_utf2wstr(p, MAX_PATH, ra->lpProvider) + 1;
		}
	}

	assert((uchar*)p-(uchar*)lpBuffer < (int)*lpBufferSize);
	*lpBufferSize = (uchar*)p-(uchar*)lpBuffer;
	
	free(buf);
	return NO_ERROR;
}


