#include <u.h>
#include <9pm/libc.h>
#include <9pm/ctype.h>
#include <9pm/srv.h>

static int	nettrans(char*, char*, int na, char*, int);

/*
 *  announce a network service.
 */
int
announce(char *addr, char *dir)
{
	int sid, clone, n;
	int conv;
	char buf[Spathlen];
	char netdir[Spathlen];
	char naddr[Spathlen];
	char *cp;

	/*
	 *  translate the address
	 */
	if(nettrans(addr, naddr, sizeof(naddr), netdir, sizeof(netdir)) < 0)
		return -1;

	/*
	 * get a control channel
	 */
	clone = srvopen(netdir, "netclone");
	if(clone<0)
		return -1;
	cp = strrchr(netdir, '/');
	*cp = 0;

	n = srvread(clone, buf, sizeof(buf)-1);
	if(n<=0) {
		srvclose(clone);
		return -1;
	}
	buf[n] = 0;
	conv = strtol(buf, &cp, 10);
	if(cp == buf || *cp != 0) {
		srvclose(clone);
		werrstr("strange rpc return value: %s", buf);
		return -1;
	}

	snprint(buf, sizeof(buf), "%s/%d", netdir, conv);
	sid = srvopen(buf, "netconv");
	srvclose(clone);	/* must close after sid is open */
	if(sid<0)
		return -1;

	/*
	 *  announce
	 */
	n = snprint(buf, sizeof(buf), "announce %s", naddr);
	if(srvrpc(sid, buf, n, 0)<0){
		srvclose(sid);
		return -1;
	}

	/*
	 *  return directory etc.
	 */
	if(dir)
		snprint(dir, Spathlen, "%s/%d", netdir, conv);
	return sid;
}

/*
 *  listen for an incoming call
 */
int
listen(int sid, char *dir)
{
	int new, n, conv;
	char buf[Snamelen];
	char netdir[Snamelen];
	char *cp;

	/*
	 *  open listen, wait for a call
	 */
	strcpy(buf, "listen");
	n = srvrpc(sid, buf, strlen(buf), sizeof(buf)-1);
	if(n<=0)
		return -1;
	buf[n] = 0;
	conv = strtol(buf, &cp, 10);
	if(cp == buf || *cp != 0) {
		werrstr("strange rpc return value: %s", buf);
		return -1;
	}
	
	strcpy(netdir, dir);
	cp = strrchr(netdir, '/');
	*cp = 0;
	snprint(buf, sizeof(buf), "%s/%d", netdir, conv);

	new = srvopen(buf, "netconv");
	if(new<0)
		return -1;

	return new;

}

/*
 *  accept a call, return an fd to the open data file
 */
int
accept(int sid)
{
	char buf[128];
	long n;

	strcpy(buf, "accept");
	n = strlen(buf);
	return srvrpc(sid, buf, n, 0); 
}

/*
 *  reject a call, tell device the reason for the rejection
 */
int
reject(int sid, char *cause)
{
	char buf[128];
	long n;

	n = sprint(buf, "reject %s", cause);
	return srvrpc(sid, buf, n, 0);
}

/*
 *  perform the identity translation (in case we can't reach cs)
 */
static int
nettrans(char *addr, char *naddr, int na, char *file, int nf)
{
	char proto[Spathlen];
	char *p;

	/* parse the protocol */
	strncpy(proto, addr, sizeof(proto));
	proto[sizeof(proto)-1] = 0;
	p = strchr(proto, '!');
	if(p)
		*p++ = 0;

	snprint(file, nf, "/%s/clone", proto);
	strncpy(naddr, p, na);
	naddr[na-1] = 0;

	return 1;
}

#if 0

/*
 *  call up the connection server and get a translation
 */
static int
nettrans(char *addr, char *naddr, int na, char *file, int nf)
{
	int i, fd;
	char buf[Spathlen];
	char netdir[Spathlen];
	char *p, *p2;
	long n;

	/*
	 *  parse, get network directory
	 */
	p = strchr(addr, '!');
	if(p == 0){
		werrstr("bad dial string: %s", addr);
		return -1;
	}
	if(*addr != '/'){
		strcpy(netdir, "/net");
	} else {
		for(p2 = p; *p2 != '/'; p2--)
			;
		i = p2 - addr;
		if(i == 0 || i >= sizeof(netdir)){
			werrstr("bad dial string: %s", addr);
			return -1;
		}
		strncpy(netdir, addr, i);
		netdir[i] = 0;
		addr = p2 + 1;
	}

	/*
	 *  ask the connection server
	 */
	sprint(buf, "%s/cs", netdir);
	fd = open(buf, ORDWR);
	if(fd < 0)
		return identtrans(netdir, addr, naddr, na, file, nf);
	if(write(fd, addr, strlen(addr)) < 0){
		close(fd);
		return -1;
	}
	seek(fd, 0, 0);
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if(n <= 0)
		return -1;
	buf[n] = 0;

	/*
	 *  parse the reply
	 */
	p = strchr(buf, ' ');
	if(p == 0)
		return -1;
	*p++ = 0;
	strncpy(naddr, p, na);
	naddr[na-1] = 0;
	strncpy(file, buf, nf);
	file[nf-1] = 0;
	return 0;
}

#endif

