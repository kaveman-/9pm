#include <u.h>
#include <9pm/libc.h>

#define	DEFB	(8*1024)
#define TMP	"install_tmp"

void	install(char *from, char *to, int todir);
void	copy1(int fdf, int fdt, char *from, char *to);
void	cleanup(char *to, int todir);
void	move(char *file);
void	fixbackslash(char *file);

int	nexttmp;

void
main(int argc, char *argv[])
{
	Dir dirb;
	int todir, i;

	if(argc<3){
		fprint(2, "usage:\t%s fromfile tofile\n", argv0);
		fprint(2, "\t%s fromfile ... todir\n", argv0);
		exits("usage");
	}

	for(i=0; i<argc; i++)
		fixbackslash(argv[i]);

	todir=0;
	if(dirstat(argv[argc-1], &dirb)==0 && (dirb.mode&CHDIR))
		todir=1;
	if(argc>3 && !todir){
		fprint(2, "cp: %s not a directory\n", argv[argc-1]);
		exits("bad usage");
	}
	cleanup(argv[argc-1], todir);
	for(i=1; i<argc-1; i++)
		install(argv[i], argv[argc-1], todir);
	exits(0);
}

void
install(char *from, char *to, int todir)
{
	Dir dirb, dirt;
	char name[256];
	int fdf, fdt;

	if(todir){
		char *s, *elem;
		elem=s=from;
		while(*s++)
			if(s[-1]=='/')
				elem=s;
		sprint(name, "%s/%s", to, elem);
		to=name;
	}
	if(dirstat(from, &dirb)!=0){
		fprint(2,"%s: can't stat %s: %r\n", argv0, from);
		return;
	}
	if(dirb.mode&CHDIR){
		fprint(2, "%s: %s is a directory\n", argv0, from);
		return;
	}
	dirb.mode &= 0777;
	if(dirstat(to, &dirt)==0)
	if(dirb.qid.path==dirt.qid.path && dirb.qid.vers==dirt.qid.vers)
	if(dirb.dev==dirt.dev && dirb.type==dirt.type){
		fprint(2, "%s: %s and %s are the same file\n", argv0, from, to);
		return;
	}
	fdf=open(from, OREAD);
	if(fdf<0){
		fprint(2, "%s: can't open %s: %r\n", argv0, from);
		return;
	}
	fdt=create(to, OWRITE, dirb.mode);
	if(fdt<0){
		move(to);
		fdt=create(to, OWRITE, dirb.mode);
	}
	if(fdt<0){
		fprint(2, "%s: can't create %s: %r\n", argv0, to);
		close(fdf);
		return;
	}
	copy1(fdf, fdt, from, to);
	close(fdf);
	close(fdt);
}

void
copy1(int fdf, int fdt, char *from, char *to)
{
	char *buf;
	long n, n1, rcount;

	buf = malloc(DEFB);
	/* clear any residual error */
	memset(buf, 0, ERRLEN);
	errstr(buf);
	for(rcount=0;; rcount++) {
		n = read(fdf, buf, DEFB);
		if(n <= 0)
			break;
		n1 = write(fdt, buf, n);
		if(n1 != n) {
			fprint(2, "%s: error writing %s: %r\n", argv0, to);
			break;
		}
	}
	if(n < 0) 
		fprint(2, "%s: error reading %s: %r\n", argv0, from);
	free(buf);
}

void
cleanup(char *to, int todir)
{
	char *name, *p;
	char file[1000];
	Dir dir[50];
	int fd, n, i, ntmp;

	name = strdup(to);
	if(!todir) {
		p = strrchr(name, '/');
		if(p == 0) {
			if(name[0] != 0 && name[1] == ':')
				name[2] = 0;
			else {
				free(name);
				name = strdup(".");
			}
		} else
			p[0] = 0;
	}
	if(dirstat(name, &dir[0]) != 0 || !(dir[0].mode&CHDIR))
		return;

	fd = open(name, OREAD|ONOSEC);
	if(fd<0)
		return;
	for(;;) {
		n = dirread(fd, dir, sizeof(dir));
		if(n <= 0)
			break;
		n /= sizeof(Dir);
		for(i=0; i<n; i++) {
			if(strncmp(dir[i].name, TMP, strlen(TMP)) != 0)
				continue;
			sprint(file, "%s/%s", name, dir[i].name);
			if(remove(file)<0) {
				/* still in use I guess */
				ntmp = atoi(dir[i].name+strlen(TMP));
				if(ntmp>=nexttmp)
					nexttmp = ntmp+1;
			}
		}
	}
	close(fd);
	free(name);
}

void
move(char *file)
{
	Dir dir;
	int i;
	char elem[NAMELEN];

	if(dirstat(file, &dir)<0)
		return;
	
	strcpy(elem, dir.name);

	/* try and move it */
	for(i=0; i<10; i++) {
		sprint(dir.name, "%s%d", TMP, nexttmp++);
		if(dirwstat(file, &dir)==0) {
			fprint(2, "%s: warning: could not overwrite %s, original moved to %s\n",
				argv0, elem, dir.name);
			return;
		}
fprint(2, "move failed %s %s: %r\n", elem, dir.name);
	}
}


void
fixbackslash(char *file)
{
	char *p;
	
	for(p=file; *p; p++)
		if(*p == '\\')
			*p = '/';
}
