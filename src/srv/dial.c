#include <u.h>
#include <9pm/libc.h>
#include <9pm/srv.h>

typedef struct DS DS;

static int	call(char *netdir, char *dest, char *local, char *dir);


/*
 *  the dialstring is of the form '[/dir/]proto!dest'
 */
int
dial(char *addr, char *local, char *dir)
{
	char netdir[Spathlen], buf[Spathlen], *p;

	strecpy(buf, addr, buf+sizeof(buf));
	p = strchr(buf, '!');
	if(p == 0) {
		werrstr("bad address");
		return -1;
	}
	*p++ = 0;
	snprint(netdir, sizeof(netdir), "/%s", buf);


	return call(netdir, p, local, dir);
}

static int
call(char *netdir, char *dest, char *local, char *dir)
{
	int clone, sid, n, conv;
	char buf[Snamelen], *cp;

	/*
	 * get a control channel
	 */
	snprint(buf, sizeof(buf), "%s/clone", netdir);
	clone = srvopen(buf, "netclone");
	if(clone<0)
		return -1;

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
	srvclose(clone);	/* don't close until sid is open */
	if(sid<0)
		return -1;

	/*
	 * connect
	 */
	if(local)
		n = snprint(buf, sizeof(buf), "connect %s %s", dest, local);
	else
		n = snprint(buf, sizeof(buf), "connect %s", dest);

	if(srvrpc(sid, buf, n, 0)<0){
		werrstr("%s failed: %r", buf);
		srvclose(sid);
		return -1;
	}

	return sid;
}

