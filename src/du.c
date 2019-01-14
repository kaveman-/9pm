#include <u.h>
#include <9pm/libc.h>

#define	ISDIR	0x80000000L

long	du(char *, Dir *);
long	k(long);
void	err(char *);
int	warn(char *);
int	seen(Dir *);
int	aflag=0;
char	fmt[] = "%lud\t%s\n";
long	blocksize = 1024;

void
main(int argc, char *argv[])
{
	int i;
	char *s, *ss;

	ARGBEGIN{
	case 'a':
		aflag=1;
		break;
	case 'b':
		s = ARGF();
		if(s) {
			blocksize = strtoul(s, &ss, 0);
			if(s == ss)
				blocksize = 1;
			if(*ss == 'k')
				blocksize *= 1024;
		}
		break;
	}ARGEND
	if(argc==0)
		print(fmt, du(".", (Dir*)0), ".");
	else
	for(i=0; i<argc; i++)
		print(fmt, du(argv[i], (Dir*)0), argv[i]);
	exits(0);
}

long
du(char *name, Dir *dir)
{
	int fd, i, n;
	Dir buf[25];
	char file[256];
	long nk, t;

	if(dir==0){
		dir=&buf[0];
		if(dirstat(name, dir)<0)
			return warn(name);
		if((dir->mode&ISDIR)==0)
			return k(dir->length);
	}
	fd=open(name, OREAD|ONOSEC);
	if(fd<0)
		return warn(name);
	nk=0;
	while((n=dirread(fd, buf, sizeof buf))>0){
		n/=sizeof(Dir);
		dir=buf;
		for(i=0; i<n; i++, dir++){
			if((dir->mode&ISDIR)==0){
				t=k(dir->length);
				nk+=t;
				if(aflag){
					sprint(file, "%s/%s", name, dir->name);
					print(fmt, t, file);
				}
				continue;
			}
			if(strcmp(dir->name, ".")==0 || strcmp(dir->name, "..")==0 || seen(dir))
				continue;
			sprint(file, "%s/%s", name, dir->name);
			t=du(file, dir);
			print(fmt, t, file);
			nk+=t;
		}
	}
	if(n<0)
		warn("name");
	close(fd);
	return nk;
}

#define	NCACHE	128	/* must be power of two */
struct cache
{
	Dir	*cache;
	int	n;
	int	max;
} cache[NCACHE];

int
seen(Dir *dir)
{
	Dir *dp;
	int i;
	struct cache *c;

	c = &cache[dir->qid.path&(NCACHE-1)];
	dp = c->cache;
	for(i=0; i<c->n; i++,dp++)
		if(dir->qid.path==dp->qid.path &&
		   dir->type==dp->type && dir->dev==dp->dev)
			return 1;
	if(c->n == c->max){
		c->cache = realloc(c->cache, (c->max+=20)*sizeof(Dir));
		if(cache == 0)
			err("malloc failure");
	}
	c->cache[c->n++] = *dir;
	return 0;
}

void
err(char *s)
{
	fprint(2, "du: ");
	perror(s);
	exits(s);
}

int
warn(char *s)
{
	fprint(2, "du: ");
	perror(s);
	return 0;
}

long
k(long n)
{
	n = (n+blocksize-1)/blocksize;
	return n*blocksize/1024;
}
