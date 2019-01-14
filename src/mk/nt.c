#include	"mk.h"
#include	<signal.h>
#include	<sys/utime.h>

#define Arc	My_Arc		/* avoid name conflicts */
#undef DELETE

#include	<windows.h>

enum {
	Nchild	= 100,
};

char *shell =		"c:\\bin\\sh.exe";
char *shellname =	"sh.exe";

typedef struct Child	Child;

struct Child {
	int	pid;
	HANDLE	handle;
};

static Child child[Nchild];

extern char **environ;

DWORD WINAPI writecmd(LPVOID a);
static Word *envtow(char*);
static char *envword(char*, char**, char**);

void
readenv(void)
{
	char **p, *s;
	Word *w;

	for(p = environ; *p; p++){
		s = shname(*p);
		if(*s == '=') {
			*s = 0;
			w = envtow(s+1);
		} else
			w = envtow("");
		if (symlook(*p, S_INTERNAL, 0))
			continue;
		s = strdup(*p);
		setvar(s, (void *)w);
		symlook(s, S_EXPORTED, (void *)"")->value = "";
	}
}

char *
exportenv(Envy *e)
{
	int i, n;
	char *buf, *v;

	buf = 0;
	n = 0;
	for(i = 0; e->name; e++, i++) {
		if(e->values)
			v = wtos(e->values);
		else
			v = "";
		buf = Realloc(buf, n+strlen(e->name)+1+strlen(v)+1);
		
		n += sprint(buf+n, "%s=%s", e->name, v);
		n++;	/* skip over null */
		if(e->values)
			free(v);
	}
	/* final null */
	buf = Realloc(buf, n+1);
	buf[n] = 0;

	return buf;
}


void
dirtime(char *dir, char *path)
{
	Dir d;
	void *t;
	char buf[4096];
	HANDLE	handle;
	WIN32_FIND_DATA	wfd;

	snprint(buf, sizeof(buf), "%s/*.*", dir);

	handle = FindFirstFile(buf, &wfd);
	if(handle == INVALID_HANDLE_VALUE)
		return;
	do {
		sprint(buf, "%s%s", path, wfd.cFileName);
		if(dirstat(buf, &d) < 0)
			continue;
		t = (void *)d.mtime;
		if (!t)			/* zero mode file */
			continue;
		if(symlook(buf, S_TIME, 0))
			continue;
		symlook(strdup(buf), S_TIME, t)->value = t;
	} while(FindNextFile(handle, &wfd) == TRUE);

	FindClose(handle);
}

int
waitfor(char *msg)
{
	int pid, n, i, r, code;
	HANDLE tab[Nchild];

	for(i=0,n=0; i<Nchild; i++)
		if(child[i].handle != 0)
			tab[n++] = child[i].handle;

	if(n == 0)
		return -1;

	r = WaitForMultipleObjects(n, tab, 0, INFINITE);

	r -= WAIT_OBJECT_0;
	if(r<0 || r>=n) {
		perror("wait failed");
		exits("wait failed");
	}

	for(i=0; i<Nchild; i++)
		if(child[i].handle == tab[r])
			break;

	if(msg) {
		*msg = 0;
		if(GetExitCodeProcess(child[i].handle, &code) == FALSE)
			snprint(msg, ERRLEN, "unknown exit code");
		else if(code != 0)
			snprint(msg, ERRLEN, "exit(%d)", code);
	}

	CloseHandle(child[i].handle);
	child[i].handle = 0;
	pid = child[i].pid;
	child[i].pid = 0;

	return pid;
}

void
expunge(int pid, char *msg)
{
/*
	if(strcmp(msg, "interrupt"))
		kill(pid, SIGINT);
	else
		kill(pid, SIGHUP);
*/
}

HANDLE
duphandle(HANDLE h)
{
	HANDLE r;

	if(DuplicateHandle(GetCurrentProcess(), h,
			GetCurrentProcess(), &r, DUPLICATE_SAME_ACCESS,
			1, DUPLICATE_SAME_ACCESS) == FALSE) {
		perror("dup handle");
		Exit();
	}

	return r;
}

void
childadd(HANDLE h, int pid)
{
	int i;
	
	for(i=0; i<Nchild; i++) {
		if(child[i].handle == 0) {
			child[i].handle = h;
			child[i].pid = pid;
			return;
		}
	}

	perror("child table full");
	Exit();
}

int
execsh(char *args, char *cmd, Bufblock *buf, Envy *e)
{
	int tot, n, tid;
	HANDLE outin, outout, inout, inin;
	struct { char *cmd; HANDLE handle; } *arg;
	char args2[4096];
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	char *eb;

	if(buf == 0)
		outout = GetStdHandle(STD_OUTPUT_HANDLE);
	else
	if(CreatePipe(&outin, &outout, 0, 0) == FALSE){
		perror("pipe");
		Exit();
	}

	if(CreatePipe(&inin, &inout, 0, 0) == FALSE){
		perror("pipe");
		Exit();
	}

	arg = malloc(sizeof(*arg));
	arg->cmd = strdup(cmd);
	arg->handle = inout;
	if(CreateThread(0, 0, writecmd, arg, 0, &tid) == FALSE) {
		perror("spawn writecmd");
		Exit();
	}

	if(args)
		if(shflags)
			sprint(args2, "%s %s %s", shellname, shflags, args);
		else
			sprint(args2, "%s %s", shellname, args);
	else if(shflags)
		sprint(args2, "%s %s", shellname, shflags);
	else
		strcpy(args2, shellname);
	
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;
	si.wShowWindow = SW_SHOW;

	if (e)
		eb = exportenv(e);
	else
		eb = 0;

	si.hStdInput = duphandle(inin);
	si.hStdOutput = duphandle(outout);
	si.hStdError = duphandle(GetStdHandle(STD_ERROR_HANDLE));
	if(CreateProcess(shell, args2, 0, 0, 1, 0, eb, 0, &si, &pi) == FALSE) {
		perror("mk fork");
		Exit();
	}

	free(eb);

	CloseHandle(si.hStdInput);
	CloseHandle(si.hStdOutput);
	CloseHandle(si.hStdError);
	CloseHandle(inin);

	childadd(pi.hProcess, pi.dwProcessId);

	if(DEBUG(D_EXEC))
		fprint(1, "starting: %s\n", cmd);

	if(buf){
		CloseHandle(outout);
		tot = 0;
		for(;;){
			if (buf->current >= buf->end)
				growbuf(buf);
			if(ReadFile(outin, buf->current, buf->end-buf->current, &n, 0) == FALSE)
				break;
			buf->current += n;
			tot += n;
		}
		if (tot && buf->current[-1] == '\n')
			buf->current--;
		CloseHandle(outin);
	}

	return pi.dwProcessId;
}

static DWORD WINAPI
writecmd(LPVOID a)
{
	struct {char *cmd; HANDLE handle;} *arg;
	char *cmd, *p;
	int n;

	arg = a;
	cmd = arg->cmd;
	p = cmd+strlen(cmd);
	while(cmd < p){
		if(WriteFile(arg->handle, cmd, p-cmd, &n, 0) == FALSE)
			break;
		cmd += n;
	}	

	free(arg->cmd);
	CloseHandle(arg->handle);
	free(arg);
	ExitThread(0);
	return 0;
}

void
Exit(void)
{
	while(waitfor(0) >= 0)
		;
	exits("error");
}

void
catchnotes()
{
}

Word*
envtow(char *s)
{
	int save;
	char *start, *end;
	Word *head, *w;

	w = head = 0;
	while(*s){
		s = envword(s, &start, &end);
		if(*start == 0)
			break;
		save = *end;
		*end = 0;
		if(w){
			w->next = newword(start);
			w = w->next;
		} else
			head = w = newword(start);
		*end = save;
	}
	if(!head)
		head = newword("");
	return(head);
}

/*
 *	break out a word from a string handling quotes
 */
static char *
envword(char *s, char **start, char **end)
{
	char *to;
	Rune r;
	int n;

	while(*s && SEP(*s))		/* skip leading white space */
		s++;
	to = *start = s;
	while(*s){
		n = chartorune(&r, s);
		if(SEP(r)){
			if (to != *start)	/* we have data */
				break;
			s += n;		/* null string - keep looking */
			while (*s && SEP(*s))	
				s++;
			to = *start = s;
		}else
			while (n--)
				*to++ = *s++;
	}
	*end = to;
	return s;
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

	if(dirstat(name, &sbuf) >= 0){
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

	*to = match->s.sp;		/* stem0 matches complete target */
	for(to++, match++; --n > 0; to++, match++){
		if(match->s.sp && match->e.ep){
			p = match->e.ep;
			c = *p;
			*p = 0;
			*to = strdup(match->s.sp);
			*p = c;
		} else
			*to = 0;
	}
}
