#include	"mk.h"
#include	<dirent.h>
#include	<signal.h>
#include	<utime.h>

char 	*shell =	"/bin/sh";
char 	*shellname =	"sh";

extern char **environ;

void
readenv(void)
{
	char **p, *s;
	Word *w;

	for(p = environ; *p; p++){
		s = shname(*p);
		if(*s == '=') {
			*s = 0;
			w = stow(s+1);
		} else
			w = stow("");
		if (symlook(*p, S_INTERNAL, 0))
			continue;
		s = strdup(*p);
		setvar(s, (void *)w);
		symlook(s, S_EXPORTED, (void *)"")->value = (void *)"";
	}
}

/*
 *	done on child side of fork, so parent's env is not affected
 *	and we don't care about freeing memory because we're going
 *	to exec immediately after this.
 */
void
exportenv(Envy *e)
{
	int i;
	char **p;
	char buf[4096];

	p = 0;
	for(i = 0; e->name; e++, i++) {
		p = (char**) Realloc(p, (i+2)*sizeof(char*));
		if(e->values)
			sprint(buf, "%s=%s", e->name,  wtos(e->values));
		else
			sprint(buf, "%s=", e->name);
		p[i] = strdup(buf);
	}
	p[i] = 0;
	environ = p;
}

void
dirtime(char *dir, char *path)
{
	DIR *dirp;
	struct dirent *dp;
	Dir d;
	void *t;
	char buf[4096];

	dirp = opendir(dir);
	if(dirp) {
		while((dp = readdir(dirp)) != 0){
			sprint(buf, "%s%s", path, dp->d_name);
			if(dirstat(buf, &d) < 0)
				continue;
			t = (void *)d.mtime;
			if (!t)			/* zero mode file */
				continue;
			if(symlook(buf, S_TIME, 0))
				continue;
			symlook(strdup(buf), S_TIME, t)->value = t;
		}
		closedir(dirp);
	}
}

int
waitfor(char *msg)
{
	int status;
	int pid;

	*msg = 0;
	pid = _wait(&status);
	if(pid > 0) {
		if(status&0x7f) {
			if(status&0x80)
				snprint(msg, ERRLEN, "signal %d, core dumped", status&0x7f);
			else
				snprint(msg, ERRLEN, "signal %d", status&0x7f);
		} else if(status&0xff00)
			snprint(msg, ERRLEN, "exit(%d)", (status>>8)&0xff);
	}
	return pid;
}

void
expunge(int pid, char *msg)
{
	if(strcmp(msg, "interrupt"))
		kill(pid, SIGINT);
	else
		kill(pid, SIGHUP);
}

int
execsh(char *args, char *cmd, Bufblock *buf, Envy *e)
{
	char *p;
	int tot, n, pid, in[2], out[2];

	if(buf && pipe(out) < 0){
		perror("pipe");
		Exit();
	}
	pid = fork();
	if(pid < 0){
		perror("mk fork");
		Exit();
	}
	if(pid == 0){
		if(buf)
			close(out[0]);
		if(pipe(in) < 0){
			perror("pipe");
			Exit();
		}
		pid = fork();
		if(pid < 0){
			perror("mk fork");
			Exit();
		}
		if(pid != 0){
			dup(in[0], 0);
			if(buf){
				dup(out[1], 1);
				close(out[1]);
			}
			close(in[0]);
			close(in[1]);
			if (e)
				exportenv(e);
			if(shflags)
				execl(shell, shellname, shflags, args, 0);
			else
				execl(shell, shellname, args, 0);
			perror(shell);
			_exits("exec");
		}
		close(out[1]);
		close(in[0]);
		p = cmd+strlen(cmd);
		while(cmd < p){
			n = write(in[1], cmd, p-cmd);
			if(n < 0)
				break;
			cmd += n;
		}
		close(in[1]);
		_exits(0);
	}
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
	return pid;
}

void
Exit(void)
{
	while(wait(0) >= 0)
		;
	exits("error");
}

int
notifyf(void *a, char *msg)
{
	static int nnote;

	USED(a);
	if(++nnote > 100){	/* until andrew fixes his program */
		fprint(2, "mk: too many notes\n");
		notify(0);
		abort();
	}
	if(strcmp(msg, "interrupt")!=0 && strcmp(msg, "hangup")!=0)
		return 0;
	killchildren(msg);
	return -1;
}

void
catchnotes()
{
	atnotify(notifyf, 1);
}

char*
maketmp(void)
{
	static char temp[] = "/tmp/mkargXXXXXX";

	mktemp(temp);
	return temp;
}

int
chgtime(char *name)
{
	Dir sbuf;
	struct utimbuf u;

	if(dirstat(name, &sbuf) >= 0) {
		u.actime = sbuf.atime;
		u.modtime = time(0);
		return utime(name, &u);
	}
	return close(create(name, OWRITE, 0666));
}

void
rcopy(char **to, Resub *match, int n)
{
	int c;
	char *p;

	*to = match->sp;		/* stem0 matches complete target */
	match;
	for(to++, match++; --n > 0; to++, match++){
		if(match->sp && match->ep){
			p = match->ep;
			c = *p;
			*p = 0;
			*to = strdup(match->sp);
			*p = c;
		} else
			*to = 0;
	}
}
