#include <u.h>
#include <9pm/libc.h>
#include <9pm/ctype.h>

static int	nettrans(char*, char*, int na, char*, int);

/*
 *  announce a network service.
 */
int
announce(char *addr, char *dir)
{
	int ctl, n, m;
	char buf[3*NAMELEN];
	char buf2[3*NAMELEN];
	char netdir[2*NAMELEN];
	char naddr[3*NAMELEN];
	char *cp;

	/*
	 *  translate the address
	 */
	if(nettrans(addr, naddr, sizeof(naddr), netdir, sizeof(netdir)) < 0)
		return -1;

	/*
	 * get a control channel
	 */
	ctl = srvopen(netdir, ORDWR);
	if(ctl<0)
		return -1;
	cp = strrchr(netdir, '/');
	*cp = 0;

	/*
	 *  find out which line we have
	 */
	n = sprint(buf, "%.*s/", 2*NAMELEN+1, netdir);
	m = read(ctl, &buf[n], sizeof(buf)-n-1);
	if(n<=0){
		close(ctl);
		return -1;
	}
	buf[n+m] = 0;

	/*
	 *  make the call
	 */
	n = sprint(buf2, "announce %.*s", 2*NAMELEN, naddr);
	if(write(ctl, buf2, n)!=n){
		close(ctl);
		return -1;
	}

	/*
	 *  return directory etc.
	 */
	if(dir)
		strcpy(dir, buf);
	return ctl;
}

/*
 *  listen for an incoming call
 */
int
listen(char *dir, char *newdir)
{
	int ctl, n, m;
	char buf[3*NAMELEN];
	char *cp;

	/*
	 *  open listen, wait for a call
	 */
	sprint(buf, "%.*s/listen", 2*NAMELEN+1, dir);
	ctl = open(buf, ORDWR);
	if(ctl < 0)
		return -1;

	/*
	 *  find out which line we have
	 */
	strcpy(buf, dir);
	cp = strrchr(buf, '/');
	*++cp = 0;
	n = cp-buf;
	m = read(ctl, cp, sizeof(buf) - n - 1);
	if(n<=0){
		close(ctl);
		return -1;
	}
	buf[n+m] = 0;

	/*
	 *  return directory etc.
	 */
	if(newdir)
		strcpy(newdir, buf);
	return ctl;

}

/*
 *  accept a call, return an fd to the open data file
 */
int
accept(int ctl, char *dir)
{
	char buf[128];
	char *num;
	long n;

	num = strrchr(dir, '/');
	if(num == 0)
		num = dir;
	else
		num++;

	sprint(buf, "accept %s", num);
	n = strlen(buf);
	write(ctl, buf, n); /* ignore return value, netowrk might not need accepts */

	sprint(buf, "%s/data", dir);
	return open(buf, ORDWR);
}

/*
 *  reject a call, tell device the reason for the rejection
 */
int
reject(int ctl, char *dir, char *cause)
{
	char buf[128];
	char *num;
	long n;

	num = strrchr(dir, '/');
	if(num == 0)
		num = dir;
	else
		num++;
	sprint(buf, "reject %s %s", num, cause);
	n = strlen(buf);
	if(write(ctl, buf, n) != n)
		return -1;
	return 0;
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
	char buf[4*NAMELEN];
	char netdir[2*NAMELEN];
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
