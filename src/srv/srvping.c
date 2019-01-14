#include <u.h>
#include <9pm/libc.h>
#include <9pm/srv.h>

static void	*sinit(void*, Sinfo*);
static int	sfini(void*);
static void	*sopen(void*, int, char*, char*, Sinfo*);
static int	sclose(void*, void*);
static int	sfree(void*, void*);
static int	srpc(void*, void*, uchar*, int, int);

Stype srvping = {
	sinit,
	sfini,
	sopen,
	sclose,
	sfree,
	0,
	0,
	srpc,
	0,
};

static void *
sinit(void *a, Sinfo *info)
{
/* fprint(2, "srvping: init\n"); /**/
	strcpy(info->name, "/");
	strcpy(info->type, "ping");
	srvgenid(info->id, &srvping);
	info->mesgsize = 0;
	
	return &srvping;	/* non zero return */
}

static int
sfini(void *a)
{
/* fprint(2, "srvping: fini\n"); /**/
	return 1;
}

static void *
sopen(void *a, int id, char *name, char *type, Sinfo *info)
{
	if(strcmp(name, "/") != 0) {	
		werrstr("unknown server");
		return 0;
	}

	if(strcmp(type, "ping") != 0) {
		werrstr("server type: ping");
		return 0;
	}
	return &srvping;
}

static int
sclose(void *a, void *id)
{
	return 1;
}

static int
sfree(void *a, void *id)
{
	return 1;
}

static int
srpc(void *a, void *id, uchar *buf, int nr, int nw)
{
	int i;
	for(i=0; i<nr; i++)
		buf[i]++;
	return nr;
}
