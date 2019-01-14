#include <u.h>
#include <9pm/libc.h>
#include <9pm/srv.h>

typedef struct Pipe	Pipe;

struct Pipe {
	Qlock	rlk;
	Qlock	wlk;
	int	fd[2];
};

static Pipe	*sinit(int*, Sinfo*);
static int	sfini(Pipe*);
static void	*sopen(Pipe*, int, char*, char*, Sinfo*);
static int	sclose(Pipe*, void*);
static int	sfree(Pipe*, void*);
static int	sread(Pipe*, void*, uchar*, int);
static int	swrite(Pipe*, void*, uchar*, int);

Stype srvpipe = {
	sinit,
	sfini,
	sopen,
	sclose,
	sfree,
	sread,
	swrite,
	0,
	0,
};

static Pipe*
sinit(int *fd, Sinfo *info)
{
	Pipe *pipe;

fprint(2, "srvpipe: init %d %d\n", fd[0], fd[1]);

	pipe = mallocz(sizeof(Pipe));
	pipe->fd[0] = fd[0];
	pipe->fd[1] = fd[1];

	strcpy(info->name, "/");
	strcpy(info->type, "link");
	srvgenid(info->id, pipe);
	info->mesgsize = 0;
	
	return pipe;
}

static int
sfini(Pipe *pipe)
{
fprint(2, "srvpipe: fini\n");
	close(pipe->fd[0]);
	close(pipe->fd[1]);
	free(pipe);

	return 1;
}

static void*
sopen(Pipe *pipe, int id, char *name, char *type, Sinfo *info)
{
	if(strcmp(name, "/") != 0 || strcmp(type, "link") != 0) {
		werrstr("unknown server");
		return 0;
	}
	return &srvpipe;
}

static int
sclose(Pipe *pipe, void *sid)
{
	return 1;
}

static int
sfree(Pipe *pipe, void *sid)
{
	return 1;
}

static int
sread(Pipe *pipe, void *sid, uchar *buf, int n)
{
	qlock(&pipe->rlk);
	n = read(pipe->fd[0], buf, n);
	qunlock(&pipe->rlk);

	return n;
}

static int
swrite(Pipe *pipe, void *sid, uchar *buf, int n)
{
	qlock(&pipe->wlk);
	n = write(pipe->fd[1], buf, n);
	qunlock(&pipe->wlk);

	return n;
}
