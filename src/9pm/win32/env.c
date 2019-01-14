#include <u.h>
#include <9pm/libc.h>
#include <9pm/windows.h>
#include "9pm.h"

enum {
	Nhash	= 101,
};

typedef struct Var Var;

struct Var {
	char	*name;
	Rune	*wstr;
	int	wstrlen;
	Var	*next;
};

struct {
	Qlock	lk;
	int	nvar;
	int	blocksize;
	Var	*hash[Nhash];
} env;

void	envload(void);
void	envadd(char *, Rune *, int);
void	envdel(char *);

void
envinit(void)
{
	Rune buf[MAX_PATH], *p;
	int n;
	HMODULE hm;
	char sysname[200], *path;

	envload();

	pm_root = getenv("9pm");
	/* if 9pm is set then assume environment is ok */
	if(pm_root)
		return;

	/* assume 9pm2.dll is in $9pm/bin */
	hm = GetModuleHandle(L"9pm2.dll");
	if(hm == 0) {
		winerror();
		fatal("envinit: could not get handle to 9pm2.dll");
	}

	n = GetModuleFileName(hm, buf, nelem(buf));
	if(n >= nelem(buf) || n == 0)
		fatal("envinit: GetModuleFileName failed");
	for(p=buf; *p; p++)
		if(*p == '\\')
			*p= '/';
	p = wstrrchr(buf, '/');
	if(p == 0)
		fatal("envinit: bad module name");
	*p = 0;
	p = wstrrchr(buf, '/');
	if(p == 0)
		fatal("envinit: bad module name");
	*p = 0;

	n = wstrutflen(buf)+1;
	pm_root = malloc(n);
	if(wstrtoutf(pm_root, buf, n) >= n)
		fatal("pm_root");
	
	putenv("9pm", pm_root);

	n = sizeof(buf);
	if(!GetComputerName(buf, &n))
		fatal("envinit: bad system name");
	wstrtoutf(sysname, buf, sizeof(sysname));
	putenv("sysname", sysname);

	path = getenv("path");
	if(path == 0)
		path = getenv("Path");
	if(path == 0)
		path = getenv("PATH");
	putenv("path", path);
	free(path);

	timezoneinit();
}


char *
getenv(char *name)
{
	Var *v;
	char *s;
	Rune *p;
	int n;
	uint h;

	for(h=0,s=name; *s; s++)
		h = h*13 + *s;
	h %= Nhash;
	s = 0;
	qlock(&env.lk);
	for(v=env.hash[h]; v; v=v->next) {
		if(strcmp(name, v->name) == 0) {
			p = wstrchr(v->wstr, '=');
			if(p == 0)
				break;
			p++;
			n = wstrutflen(p);
			s = malloc(n+1);
			wstrtoutf(s, p, n+1);
			break;
		}
	}
	qunlock(&env.lk);
		
	return s;
}


int
putenv(char *name, char *val)
{
	Rune *w, *p;
	int n;

	if(val != 0) {
		n = utflen(name)+1+utflen(val);
		w = malloc((n+1)*sizeof(Rune));
		p = w;
		p += utftowstr(p, name, n+1);
		*p++ = '=';
		p += utftowstr(p, val, n+1);
		assert(p-w == n);
		envadd(name, w, n);
	} else
		envdel(name);
	return 0;
}

void
envload(void)
{
	Rune *envblk, *s, *w;
	char name[100], *p, *e;
	int n;

	env.blocksize = 1;

	envblk = GetEnvironmentStrings();
	if(envblk == 0)
		return;
	for(s=envblk; *s; s+=n+1) {
		n = wstrlen(s);
		e = name+sizeof(name)-UTFmax;
		for(w=s,p=name; *w && *w != '=' && p<e; w++)
			p += runetochar(p, w);
		*p = 0;
		w = malloc((n+1)*sizeof(Rune));
		memcpy(w, s, (n+1)*sizeof(Rune));
		envadd(name, w, n);
	}

	FreeEnvironmentStrings(envblk);
}

char**
getenvall(void)
{
	char **envp, **ep, *p;
	Var *v;
	int i, j, n;

	qlock(&env.lk);
	envp = malloc((env.nvar+1)*sizeof(char*));
	for(i=0,ep=envp; i<Nhash; i++) {
		for(v=env.hash[i]; v; v=v->next) {
			*ep = malloc(wstrutflen(v->wstr)+1);
			n = v->wstrlen;
			for(j=0,p=*ep; j<n; j++)
				p += runetochar(p, v->wstr+j);
			*p = 0;
			ep++;
		}
	}
	*ep = 0;
	assert(ep-envp == env.nvar);
	qunlock(&env.lk);
	return envp;
}

Rune *
getenvblock(void)
{
	Rune *blk, *p;
	Var *v;
	int i;

	qlock(&env.lk);
	blk = malloc(env.blocksize*sizeof(Rune));
	for(i=0,p=blk; i<Nhash; i++) {
		for(v=env.hash[i]; v; v=v->next) {
			memcpy(p, v->wstr, (v->wstrlen+1)*sizeof(Rune));
			p += v->wstrlen+1;
		}
	}
	*p++ = 0;
	assert(p-blk == env.blocksize);
	qunlock(&env.lk);
	return blk;
}


void
envadd(char *name, Rune *wstr, int n)
{
	Var *v;
	uint h;
	char *s;

	for(h=0,s=name; *s; s++)
		h = h*13 + *s;
	h %= Nhash;
	s = 0;

	qlock(&env.lk);
	for(v=env.hash[h]; v; v=v->next)
		if(strcmp(name, v->name) == 0)
			break;
	if(v) {
		env.blocksize -= v->wstrlen;
		free(v->wstr);
		env.blocksize += n;
	} else {
		v = malloc(sizeof(Var));
		v->name = strdup(name);
		v->next = env.hash[h];
		env.hash[h] = v;
		env.nvar++;
		env.blocksize += n+1;
	}
		
	v->wstr = wstr;
	v->wstrlen = n;

	qunlock(&env.lk);
}

void
envdel(char *name)
{
	Var *v, *ov;
	uint h;
	char *s;

	for(h=0,s=name; *s; s++)
		h = h*13 + *s;
	h %= Nhash;
	s = 0;
	qlock(&env.lk);
	for(v=env.hash[h],ov=0; v; ov=v,v=v->next) {
		if(strcmp(name, v->name) != 0)
			continue;
		env.nvar--;
		env.blocksize -= v->wstrlen+1;
		if(ov)
			ov->next = v->next;
		else
			env.hash[h] = v;
		free(v->name);
		free(v->wstr);
		free(v);
		break;
	}
	qunlock(&env.lk);
}
