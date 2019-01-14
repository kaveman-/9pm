#include	"mk.h"

enum {
	Nchild	= 100,
};

typedef struct Child	Child;

struct Child {
	int	pid;
	int	done;
	char	err[ERRLEN];
};

struct {
	Qlock	lk;
	Event	event;
	Child	child[Nchild];
} slave;

static	void	writecmd(void*);
static	void	procwaitthread(void*);

char *shell =		"rcsh.exe";
char *shellname =	"rcsh.exe";

void
readenv(void)
{
	char **env, *name, *val, *p;
	int i;
	Word *w, *nw;
	int n;

	env = getenvall();

	for(i=0; env[i]; i++) {
		p = strchr(env[i], '=');
		if(p == 0 || p == env[i])
			continue;
		*p = 0;
		name = strdup(env[i]);
		val = p+1;
		n = strlen(val);
		w = 0;
		p = val+n-1;
		while(*p) {
			if(*p == 0x01)
				*p-- = 0;
			for(; *p && *p != 0x01; p--)
				;
			nw = newword(p+1);
			nw->next = w;
			w = nw;
		}
		if(w == 0)
			continue;
		setvar(name, (char*)w);
		symlook(name, S_EXPORTED, "")->value = "";
	}

	for(i=0; env[i]; i++)
		free(env[i]);
	free(env);
}


char **
exportenv(Envy *e)
{
	int i, n, n2;
	char **envp, *p;
	Word *w;
	
	for(i=0; e[i].name; i++)
		;
	n = i;
	envp = malloc((n+1)*sizeof(char*));
	for(i=0; i<n; i++) {
		n2 = strlen(e[i].name)+1;
		for (w = e[i].values; w; w = w->next)
			n2 += strlen(w->s)+1;
		envp[i] = malloc(n2);
		p = envp[i];
		p += sprint(p, "%s=", e[i].name);
		for (w = e[i].values; w; w = w->next) {
			p += sprint(p, "%s", w->s);
			if(w->next)
				*p++ = 01;
		}
	}
	/* final null */
	envp[n] = 0;

	return envp;
}

void
dirtime(char *dir, char *path)
{
	int i, fd, n;
	char *t;
	Dir db[32];
	char buf[4096];

	fd = open(dir, OREAD|ONOSEC);
	if(fd >= 0) {
		while((n = dirread(fd, db, sizeof db)) > 0){
			n /= sizeof(Dir);
			for(i = 0; i < n; i++){
				t = (char *)db[i].mtime;
				if (!t)			/* zero mode file */
					continue;
				sprint(buf, "%s%s", path, db[i].name);
				if(symlook(buf, S_TIME, 0))
					continue;
				symlook(strdup(buf), S_TIME, t)->value = t;
			}
		}
		close(fd);
	}
}

int
waitfor(char *msg)
{
	int i, n;
	int pid;
	Child *c;

	qlock(&slave.lk);

	for(i=0,n=0; i<Nchild; i++)
		if(slave.child[i].pid != 0)
			n++;

	if(n == 0) {
		qunlock(&slave.lk);
		return -1;
	}

	for(;;) {
		for(i=0,c=slave.child; i<Nchild; i++,c++)
			if(c->done)
				break;
		if(i<Nchild)
			break;
		qunlock(&slave.lk);
		ewait(&slave.event);
		qlock(&slave.lk);
	}

	c->done = 0;
	if(msg)
		strcpy(msg, c->err);
	pid = c->pid;
	c->pid = 0;
	qunlock(&slave.lk);
	
	return pid;		
}

void
expunge(int pid, char *msg)
{
}

int
execsh(char *args, char *cmd, Bufblock *buf, Envy *e)
{
	int tot, n, in[2], out[2];
	int i;
	struct { char *cmd; int fd;} *wt;
	Child *c;
	char *argv[4];
	char **envp;

	out[1] = 1;
	if(buf && pipe(out) < 0) {
		perror("pipe");
		Exit();
	}

	if(pipe(in) < 0){
		perror("pipe");
		Exit();
	}

	wt = malloc(sizeof(*wt));
	wt->cmd = strdup(cmd);
	wt->fd = in[1];
	if(!thread(writecmd, wt)) {
		perror("spawn writecmd");
		Exit();
	}

	envp = 0;
	if(e)
		envp = exportenv(e);

	i=0;
	argv[i++] = shellname;
	if(shflags)
		argv[i++] = shflags;
	if(args) 
		argv[i++] = args;
	argv[i++] = 0;

	qlock(&slave.lk);
	for(i=0; i<Nchild; i++)
		if(slave.child[i].pid == 0)
			break;
	assert("child table full", i<Nchild);
	c = slave.child+i;
	
	if((c->pid = proc(argv, in[0], out[1], 2, envp)) == 0) {
		perror("proc failed");
		exits("proc failed");
	}
	qunlock(&slave.lk);

	close(in[0]);

	if(envp) {
		for(i=0; envp[i]; i++)
			free(envp[i]);
		free(envp);
	}

	thread(procwaitthread, c);


	if(buf){
		close(out[1]);
		tot = 0;
		for(;;){
			if (buf->current >= buf->end)
				growbuf(buf);
			n = read(out[0], buf->current, buf->end-buf->current);
			if(n <= 0)
				break;
			buf->current += n;
			tot += n;
		}
		if (tot && buf->current[-1] == '\n')
			buf->current--;
		close(out[0]);
	}
	return c->pid;
} 

void
writecmd(void *a)
{
	struct {char *cmd; int fd;} *arg;
	char *p, *e;
	int n;
	arg = a;
	p = arg->cmd;
	e = p+strlen(p);
	while(p < e){
		n = write(arg->fd, p, e-p);
		if(n <= 0)
			break;
		p += n;
	}	
	free(arg->cmd);
	close(arg->fd);
	free(arg);
}

void
procwaitthread(void *a)
{
	Child *c;
	int err;

	c = a;
	c->err[0] = 0;
	err = procwait(c->pid, 0);
	if(err)
		sprint(c->err, "error code %d", err);
	c->done = 1;
	esignal(&slave.event);
}

void
Exit(void)
{
	while(waitfor(0) != -1)
		;
	exits("error");
}

int
notifyf(void *a, char *msg)
{
	return -1;
}

void
catchnotes()
{
}

char*
maketmp(void)
{
	static char temp[] = "/tmp/mkargXXXXXX";

	mktemp(temp);
	return temp;
}

void
rcopy(char **to, Resub *match, int n)
{
	int c;
	char *p;

	*to++ = match->sp;		/* stem0 matches complete target */
	match++;
	while (--n > 0) {
		if(match->sp && match->ep) {
			p = match->ep;
			c = *p;
			*p = 0;
			*to = strdup(match->sp);
			*p = c;
		}
		else
			*to = 0;
	}
}

int
chgtime(char *name)
{
	Dir sbuf;

	if(dirstat(name, &sbuf) >= 0) {
		sbuf.mtime = time((long *)0);
		return dirwstat(name, &sbuf);
	}
	return close(create(name, OWRITE, 0666));
}
