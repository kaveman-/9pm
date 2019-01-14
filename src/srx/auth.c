#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include "auth.h"

#pragma comment( lib, "advapi32.lib")

static char *badreq = "bad ticket request";
static char *ccmsg = "can't connect to AS";
static char *srmsg = "server refused authentication";
static char *sgmsg = "server gave up";

enum {
	AuthPort = 567,
};

int
auth(int so)
{
	int n, aso;
	int rv;
	char trbuf[TICKREQLEN];
	char tbuf[2*TICKETLEN+AUTHENTLEN];
	char key[DESKEYLEN];
	char authsrv[AUTHSRVLEN];
	Ticketreq tr;
	Ticket t;
	Authenticator a;

	/* add uid and local hostid to ticket request */
	if(_asreadn(so, trbuf, TICKREQLEN) < 0){
		blSetError("auth", badreq);
		return -1;
	}
	convM2TR(trbuf, &tr);
	if(tr.type != AuthTreq){
		blSetError("auth", badreq);
		return -1;
	}
	memset(tr.uid, 0, sizeof(tr.uid));
	n = sizeof(tr.uid);
	if(!GetUserName(tr.uid, &n)) {
		blOSError("GetUserName");
		return -1;
	}
	
	sprintf(authsrv, "p9auth.%s", tr.authdom);
	memset(key, 0, sizeof(key));
	if(!prompt(tr.uid, key, authsrv))
		return -1;

//	if(checkkey(tr.uid, key) < 0)
//		blFatal("bad key: %s", blGetError());

	memset(tr.hostid, 0, sizeof(tr.hostid));
	strcpy(tr.hostid, tr.uid);
	convTR2M(&tr, trbuf);

	aso = dial(authsrv, AuthPort);
	if(aso < 0)
		blFatal("could not dial: %s: %s", authsrv, blGetError());
	rv = _asgetticket(aso, trbuf, tbuf);
	closesocket(aso);

	if(rv < 0){
		blClearError();
		blSetError("auth", "getticket");
		return -1;
	}

	/* get authenticator */
	convM2T(tbuf, &t, key);
	if(t.num != AuthTc || strcmp(t.cuid, tr.uid)){
		blClearError();
		blSetError("auth", "decrypt ticket");
		return -1;
	}
	a.num = AuthAc;
	a.id = 0;
	memmove(a.chal, t.chal, CHALLEN);
	convA2M(&a, tbuf+2*TICKETLEN, t.key);

	/* write server ticket to server */
	if(send(so, tbuf+TICKETLEN, TICKETLEN+AUTHENTLEN, 0) < 0){
		blClearError();
		blSetError("auth", srmsg);
		return -1;
	}

	/* get authenticator from server and check */
	if(_asreadn(so, tbuf+2*TICKETLEN, AUTHENTLEN) < 0){
		blClearError();
		blSetError("auth", srmsg);
		return -1;
	}

	memset(tbuf, 0, AUTHENTLEN);
	if(memcmp(tbuf, tbuf+2*TICKETLEN, AUTHENTLEN) == 0) {
		blClearError();
		blSetError("auth", "refused by server");
		return -1;
	}

	convM2A(tbuf+2*TICKETLEN, &a, t.key);
	if(a.num != AuthAs || memcmp(t.chal, a.chal, CHALLEN) || a.id != 0) {
		blClearError();
		blSetError("auth", "server lies");
		return -1;
	}
	return 0;
}

int
_asreadn(int so, char *buf, int len)
{
	int m, n;

	for(n = 0; n < len; n += m){
		m = recv(so, buf+n, len-n, 0);
		if(m <= 0)
			return -1;
	}
	return n;
}

int
_asgetticket(int fd, char *trbuf, char *tbuf)
{
	if(send(fd, trbuf, TICKREQLEN, 0) < 0){
		blSetError("auth", "AS protocol botch");
		return -1;
	}
	return _asrdresp(fd, tbuf, 2*TICKETLEN);
}

int
_asrdresp(int fd, char *buf, int len)
{
	int n;
	char error[ERRLEN];

	if(recv(fd, buf, 1, 0) != 1){
		blSetError("auth", "AS protocol botch");
		return -1;
	}

	n = len;
	switch(buf[0]){
	case AuthOK:
		if(_asreadn(fd, buf, len) < 0){
			blSetError("auth", "AS protocol botch");
			return -1;
		}
		break;
	case AuthErr:
		if(_asreadn(fd, error, ERRLEN) < 0){
			blSetError("auth", "AS protocol botch");
			return -1;
		}
		error[ERRLEN-1] = 0;
		blSetErrorEx("auth", "remote", error);
		return -1;
	case AuthOKvar:
		if(_asreadn(fd, error, 5) < 0){
			blSetError("auth", "AS protocol botch");
			return -1;
		}
		error[5] = 0;
		n = atoi(error);
		if(n <= 0 || n > len){
			blSetError("auth", "AS protocol botch");
			return -1;
		}
		memset(buf, 0, len);
		if(_asreadn(fd, buf, n) < 0){
			blSetError("auth", "AS protocol botch");
			return -1;
		}
	default:
		blSetError("auth", "AS protocol botch");
		return -1;
	}
	return n;
}
