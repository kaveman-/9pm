#include <u.h>
#include <9pm/libc.h>
#include <9pm/windows.h>
#include "9pm.h"

int	usesecurity;
int	tmpconsole;

static int	dopipe(int fd[2], int eof);
static HANDLE	serveropen(Rune *path);
static int	serverread(File *f, uchar *buf, int n);
static void	edirset(char *edir, WIN32_FIND_DATA *data, Rune *dpath, int);
static int	dirbadentry(WIN32_FIND_DATA *data);
static int	dirnext(File *f, WIN32_FIND_DATA *data);
static File	*filealloc(void);
static long	unixtime(FILETIME ft);
static FILETIME wintime(ulong t);
static int	pathqid(int, Rune*);
static int	consolewrite(File *f, uchar *buf, int n);
static int	consoleread(File *f, uchar *buf, int n);
static int	dirdoread(File*, uchar *buf, int n);
static int	dostat(char*, char*, int);

File	fdtbl[Nfd];

void
fdinit(void)
{
	int i, j;
	char *p;

	for(i=0; i<Nfd; i++)
		fdtbl[i].type = Tfree;

	fdtbl[0].h = GetStdHandle(STD_INPUT_HANDLE);
	fdtbl[0].mode = OREAD;
	fdtbl[1].h = GetStdHandle(STD_OUTPUT_HANDLE);
	fdtbl[1].mode = OWRITE;
	fdtbl[2].h = GetStdHandle(STD_ERROR_HANDLE);
	fdtbl[2].mode = OWRITE;
	for(i=0; i<3; i++) {
		if(fdtbl[i].h == INVALID_HANDLE_VALUE || fdtbl[i].h == 0) {
			fdtbl[i].type = Tdevnull;
			continue;
		}
		if(GetConsoleMode(fdtbl[i].h, &j)) {
			fdtbl[i].type = Tconsole;
			continue;
		}
		j = GetFileType(fdtbl[i].h);
		switch(j) {
		default:
			fatal("unknown file type: %d", j);
		case FILE_TYPE_UNKNOWN:
			fdtbl[i].type = Tdevnull;
			break;
		case FILE_TYPE_DISK:
		case FILE_TYPE_CHAR:
			fdtbl[i].type = Tfile;
			break;
		case FILE_TYPE_PIPE:
			fdtbl[i].type = Tpipe;
			break;
		}
	}


	/* convert devnull on stdout and stderr to lazy consoles */
//	for(i=1; i<3; i++) {
//		if(fdtbl[i].type != Tdevnull)
//			continue;
//		fdtbl[i].type = Tconsole;
//		fbtbl[i].mode = OREAD;
//		fdtbl[i].h = INVALID_HANDLE_VALUE;
//	}
	

	/* hack to enable pipes to recognize control d */
	if(fdtbl[0].type == Tpipe) {
		p = getenv("stdin-pipe");
		if(p && strcmp(p, "interactive") == 0)
			fdtbl[0].type = Tstdin;
		free(p);
	}

	if(usesecurity)
		secinit();
}


int
open(char *file, int mode)
{
	Rune path[MAX_PATH];
	int attr, t, acc, share, cflag, aflag;
	HANDLE h;
	File *f;

	if(strcmp(file, "stdin$") == 0)
		return dup(0, -1);
	if(strcmp(file, "stdout$") == 0)
		return dup(1, -1);
	if(strcmp(file, "stderr$") == 0)
		return dup(2, -1);

	t = setpath(path, file);
	
	switch(t) {
	case Pbad:
		return -1;
	case Pfile:
	case Pvolume:
		attr = GetFileAttributes(path);
	
		if(attr <  0) {
			winerror();
			return -1;
		}

		if(attr & FILE_ATTRIBUTE_DIRECTORY) {
			if((f = filealloc()) == 0)
				return -1;
			f->type = Tdir;
			f->h = INVALID_HANDLE_VALUE;
		} else {
			switch(mode&0x3) {
			default:
				acc = 0;
			case OREAD:
			case OEXEC:
				acc = GENERIC_READ;
				break;
			case OWRITE:
				acc = GENERIC_WRITE;
				break;
			case ORDWR:
				acc = GENERIC_READ|GENERIC_WRITE;
				break;
			}
			cflag = OPEN_EXISTING;
			if(mode&OTRUNC)
				cflag = TRUNCATE_EXISTING;
			
			aflag = 0;
			if(mode&ORCLOSE)
				aflag |= FILE_FLAG_DELETE_ON_CLOSE;

			share = FILE_SHARE_READ|FILE_SHARE_WRITE;
 
			h = CreateFile(path, acc, share, 0, cflag, aflag, 0);
			if(h == INVALID_HANDLE_VALUE) {
				winerror();
				return -1;
			}
			if((f = filealloc()) == 0) {
				CloseHandle(h);
				return -1;
			}
			f->type = Tfile;
			f->h = h;
		}
		break;
	case Pserver:
		if((h = serveropen(path)) == INVALID_HANDLE_VALUE)
			return -1;
		if((f = filealloc()) == 0) {
			CloseHandle(h);
			return -1;
		}
		f->type = Tserver;
		f->h = h;
		break;
	}

	f->mode = mode;
	f->path = wstrdup(path);
	f->qid = pathqid(0, f->path);
	qunlock(&f->lk);
		
	return f-fdtbl;
}

int
create(char *file, int mode, ulong perm)
{
	Rune path[MAX_PATH];
	int t, acc, share, cflag, aflag;
	File *f;
	HANDLE h;

	t = setpath(path, file);
	if(t == Pbad)
		return -1;

	if(t == Pvolume || t == Pserver) {
		werrstr("invalid file name");
		return -1;
	}

	if(perm & CHDIR) {
		if(mode != OREAD) {
			werrstr("wrong mode");
			return -1;
		}
		/* need to do permission correctly here */
		if(!CreateDirectory(path, 0)) {
			winerror();
			return -1;
		}
		if((f = filealloc()) == 0)
			return -1;
		f->type = Tdir;
		f->h = INVALID_HANDLE_VALUE;
	} else {
	
		switch(mode&0x3) {
		default:
			acc = 0;
		case OREAD:
		case OEXEC:
			acc = GENERIC_READ;
			break;
		case OWRITE:
			acc = GENERIC_WRITE;
			break;
		case ORDWR:
			acc = GENERIC_READ|GENERIC_WRITE;
			break;
		}
		cflag = CREATE_ALWAYS;
		
		aflag = 0;
		if(mode&ORCLOSE)
			aflag |= FILE_FLAG_DELETE_ON_CLOSE;
	
		share = FILE_SHARE_READ|FILE_SHARE_WRITE;
	
		h = CreateFile(path, acc, share, 0, cflag, aflag, 0);
	
		if(h == INVALID_HANDLE_VALUE) {
			winerror();
			return -1;
		}
		if((f = filealloc()) == 0) {
			CloseHandle(h);
			return -1;
		}
		f->type = Tfile;
		f->h = h;
	}
	f->mode = mode;
	f->path = wstrdup(path);
	f->qid = pathqid(0, f->path);
	qunlock(&f->lk);
		
	return f-fdtbl;
}

int
dup(int oldfd, int newfd)
{
	File *nf, *of;

	if(oldfd < 0 || oldfd >= Nfd) {
		werrstr("fd out of range");
		return -1;
	}

	if(newfd >= Nfd) {
		werrstr("fd out of range");
		return -1;
	}

	if(newfd < 0) {
		if((nf = filealloc()) == 0)
			return -1;
	} else {
		if(newfd == oldfd)
			return newfd;
		nf = &fdtbl[newfd];
		qlock(&nf->lk);
		/* close it down */
		
	}

	of = &fdtbl[oldfd];
	qlock(&of->lk);
	if(of->path)
		nf->path = wstrdup(of->path);
	nf->type = of->type;
	nf->qid = of->qid;
	nf->offset = of->offset;
	nf->mode = of->mode;

	switch(of->type) {
	default:
		fatal("dup: unknown fd type: %d", of->type);
	case Tpipe:
	case Tconsole:
	case Tstdin:
	case Tfile:
		if(!DuplicateHandle(GetCurrentProcess(), of->h,
				GetCurrentProcess(), &nf->h, DUPLICATE_SAME_ACCESS,
				0, DUPLICATE_SAME_ACCESS)) {
			
			fatal("dup");
		}
		break;
	}

	qunlock(&of->lk);
	qunlock(&nf->lk);

	return nf-fdtbl;
}

int
pipe(int fd[2])
{
	return dopipe(fd, 0);
}

int
pipeeof(int fd[2])
{
	return dopipe(fd, 1);
}

int
dopipe(int fd[2], int eof)
{
	File *f0, *f1;
	HANDLE h0, h1;

	if(!CreatePipe(&h0, &h1, 0, 0)) {
		winerror();
		return -1;
	}

	if((f0 = filealloc()) == 0) {
Err:
		CloseHandle(h0);
		CloseHandle(h1);
		return -1;
	}

	if(eof)
		f0->type = Tstdin;
	else
		f0->type = Tpipe;
	f0->h = h0;
	f0->mode = OREAD;

	if((f1 = filealloc()) == 0) {
		f0->type = Tfree;
		qunlock(&f0->lk);
		goto Err;
	}

	f1->type = Tpipe;
	f1->h = h1;
	f1->mode = OWRITE;

	qunlock(&f0->lk);
	qunlock(&f1->lk);

	fd[0] = f0-fdtbl;
	fd[1] = f1-fdtbl;

	return 0;
}


int
close(int fd)
{
	File *f;
	int r;

	if(fd < 0 || fd >= Nfd) {
		werrstr("fd out of range");
		return -1;
	}
	
	f = &fdtbl[fd];
	qlock(&f->lk);
	r = 0;
	switch(f->type) {
	default:
		fatal("close: unknown fd type: %d", f->type);
	case Tfree:
		werrstr("invalid fd");
		r = -1;
		break;
	case Tconsole:
	case Tpipe:
	case Tfile:
	case Tstdin:
		if(f->h != INVALID_HANDLE_VALUE)
			CloseHandle(f->h);
		break;
	case Tdir:
		FindClose(f->h);
		break;
	case Tserver:
		WNetCloseEnum(f->h);
		break;
	}
	free(f->path);
	f->type = Tfree;
	/* zero every thing after type */
	memset(&f->path, 0, sizeof(File)-(int)&((File*)0)->path);
	f->h = INVALID_HANDLE_VALUE;
	qunlock(&f->lk);
	return r;
}

long
read(int fd, void *buf, long n)
{
	int n2, r;
	File *f;
	HANDLE h;
	uchar *p;

	if(n == 0)
		return 0;

	if((f = filelookup(fd)) == 0)
		return -1;
	qlock(&f->lk);

	if((f->mode&3) == OWRITE) {
		qunlock(&f->lk);
		werrstr("file not opened for reading");
		return -1;
	}

	if(f->eof) {
		qunlock(&f->lk);
		return 0;
	}


	switch(f->type) {
	default:
		fatal("unknown file type: %d", f->type);
	case Tfree:
		qunlock(&f->lk);
		werrstr("invalid fd");
		return -1;
	case Tstdin:
		h = f->h;
		r = ReadFile(f->h, buf, n, &n2, 0);
		if(r == 0) {
			winerror();
			qunlock(&f->lk);
			return 0;	/* eof - not error */
		}
		/* look for control d */
		for(p=buf; p<(uchar*)buf+n2; p++) {
			if(*p == '\004') {
				n2 = p-(uchar*)buf;
				f->eof = 1;
				break;
			}
		}		
		qunlock(&f->lk);
		break;
	case Tpipe:
		h = f->h;
		qunlock(&f->lk);
		r = ReadFile(h, buf, n, &n2, 0);
		if(r == 0) {
			winerror();
			return 0;	/* eof - not error */
		}
		break;
	case Tfile:
		h = f->h;
		qunlock(&f->lk);
		r = ReadFile(h, buf, n, &n2, 0);
		if(r == 0) {
			winerror();
			return -1;
		}
		break;
	case Tconsole:
		h = f->h;
		n2 = consoleread(f, buf, n);
		qunlock(&f->lk);
		break;
	case Tdir:
		n2 = dirdoread(f, buf, n);
		qunlock(&f->lk);
		break;
	case Tserver:
		n2 = serverread(f, buf, n);
		qunlock(&f->lk);
		break;
	case Tdevnull:
		f->eof = 1;
		n2 = 0;
		qunlock(&f->lk);
		break;
	}

	return n2;
}


long
readx(int fd, void *buf, long n, long offset)
{
	seek(fd, offset, 0);
	return read(fd, buf, n);
}

long
write(int fd, void *buf, long n)
{
	int n2, r;
	File *f;
	HANDLE h;

	if((f = filelookup(fd)) == 0)
		return -1;

	qlock(&f->lk);

	if((f->mode&3) != OWRITE && (f->mode&3) != ORDWR) {
		qunlock(&f->lk);
		werrstr("file not opened for writting");
		return -1;
	}

	switch(f->type) {
	default:
		fatal("unknown file type: %d", f->type);
	case Tfree:
		qunlock(&f->lk);
		werrstr("invalid fd");
		return -1;
	case Tpipe:
	case Tfile:
		h = f->h;
		qunlock(&f->lk);
		r = WriteFile(f->h, buf, n, &n2, 0);
		if(r == 0) {
			winerror();
			return -1;
		}
		break;
	case Tconsole:
		n2 = consolewrite(f, buf, n);
		qunlock(&f->lk);
		break;
	case Tdir:
		qunlock(&f->lk);
		werrstr("can not write directories");
		return -1;
	case Tdevnull:
		qunlock(&f->lk);
		return n;
	}

	return n2;
}

long
writex(int fd, void *buf, long n, long offset)
{
	seek(fd, offset, 0);
	return write(fd, buf, n);
}

int
remove(char *file)
{
	Rune path[MAX_PATH];
	int attr, t;

	t = setpath(path, file);
	if(t == Pbad)
		return -1;
	if(t != Pfile) {
		werrstr("cannot remove file");
		return -1;
	}

	attr = GetFileAttributes(path);
	
	if(attr <  0) {
		winerror();
		return -1;
	}

	if(attr & FILE_ATTRIBUTE_DIRECTORY) {
		if(!RemoveDirectory(path)) {
			winerror();
			return -1;
		}
	} else {
		if(!DeleteFile(path)) {
			winerror();
			return -1;
		}
	}
	return 0;
}

int
chdir(char *dirname)
{
	Rune path[MAX_PATH];
	int t, r;

	t = setpath(path, dirname);
	switch(t) {
	default:
		fatal("chdir: unknown path name");
	case Pbad:
		return -1;
	case Pvolume:
		wstrcat(path, L"\\");
		r = SetCurrentDirectory(path);
		if(!r) {
			winerror();
			return -1;
		}
		break;
	case Pfile:
		r = SetCurrentDirectory(path);
		if(!r) {
			winerror();
			return -1;
		}
		break;
	case Pserver:
		werrstr("can't chdir to server yet");
		return -1;
	}
	return 0;
}

char*
getwd(char *buf, int size)
{
	int n;
	char *p;
	Rune buf2[MAX_PATH];

	buf[0] = 0;

	n = GetCurrentDirectory(nelem(buf2), buf2);
	if(n == 0) {
		winerror();
		return 0;
	}

	n = wstrtoutf(buf, buf2, size);
	if(n >= size) {
		werrstr("buffer too small");
		buf[0] = 0;
		return 0;
	}

	for(p=buf; *p; p++)
		if(*p == '\\')
			*p = '/';

	return buf;
}


int
dostat(char *file, char *edir, int nosec)
{	
	HANDLE h;
	WIN32_FIND_DATA data;
	Rune path[MAX_PATH], *p;
	int t;
	Dir dir;

	t = setpath(path, file);
	switch(t) {
	default:
		fatal("unknown path type: %d", t);
	case Pbad:
		return -1;
	case Pvolume:
		data.dwFileAttributes = GetFileAttributes(path);
		if(data.dwFileAttributes == 0xffffffff){
			winerror();
			return -1;
		}
		data.ftCreationTime =
		data.ftLastAccessTime =
		data.ftLastWriteTime = wintime(time(0));
		data.nFileSizeHigh = 0;
		data.nFileSizeLow = 0;
		p = wstrrchr(path, '\\');
		*p = 0;
		wstrcpy(data.cFileName, p+1);
		edirset(edir, &data, path, nosec);
 		break;

	case Pfile:
		memset(&data, 0, sizeof(data));
		h = FindFirstFile(path, &data);
		if(h == INVALID_HANDLE_VALUE) {
			winerror();
			return -1;
		}
		FindClose(h);
		p = wstrrchr(path, '\\');
		*p = 0;

		edirset(edir, &data, path, nosec);
		break;
	case Pserver:
		p = wstrrchr(path, '\\');
		memset(&dir, 0, sizeof(dir));
		if(p == 0)
			wstrtoutf(dir.name, path, sizeof(dir.name));
		else
			wstrtoutf(dir.name, p+1, sizeof(dir.name));
		strcpy(dir.uid, "none");
		strcpy(dir.gid, "none");
		dir.mtime = dir.atime = time(0);
		dir.type = 'v';
		dir.dev = 0;
		dir.qid.path = pathqid(0, path) | CHDIR;
		dir.qid.vers = dir.mtime;
		dir.mode = 0777|CHDIR;
		convD2M(&dir, edir);
		break;
	}

	return 0;
}

int
stat(char *file, char *edir)
{
	return dostat(file, edir, 0);
}

int
fstat(int fd, char *edir)
{
	int r;
	File *f;
	char buf[MAX_PATH];

	if((f = filelookup(fd)) == 0)
		return -1;

	qlock(&f->lk);

	switch(f->type) {
	default:
		qunlock(&f->lk);
		werrstr("bad file type");
		break;
	case Tfile:
	case Tdir:
		wstrtoutf(buf, f->path, sizeof(buf));
		r = dostat(buf, edir, f->mode&ONOSEC);
		qunlock(&f->lk);
		return r;
	}
	return -1;
}

int
wstat(char *name, char *edir)
{
	HANDLE h;
	WIN32_FIND_DATA data;
	Rune path[MAX_PATH], newpath[MAX_PATH], *p;
	int t, n;
	Dir dir;
	FILETIME atime, mtime;
	uint attr;

	convM2D(edir, &dir);
	t = setpath(path, name);
	switch(t) {
	default:
		fatal("unknown path type: %d", t);
	case Pbad:
		return -1;
	case Pvolume:
		if(usesecurity && secwperm(path, &dir, 1) < 0) {
fprint(2, "wstat: secwperm failed: %r\n");
			return -1;
		}
		break;
	case Pfile:
		h = FindFirstFile(path, &data);
		if(h == INVALID_HANDLE_VALUE) {
			winerror();
			return -1;
		}
		FindClose(h);

		if(unixtime(data.ftLastAccessTime) != dir.atime ||
		   unixtime(data.ftLastWriteTime) != dir.mtime) {
			h = CreateFile(path, GENERIC_WRITE,
				0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
			if(h == INVALID_HANDLE_VALUE) {
				winerror();
				return -1;
			}
			mtime = wintime(dir.mtime);
			atime = wintime(dir.atime);
			if(!SetFileTime(h, 0, &atime, &mtime)) {
				winerror();
				CloseHandle(h);
				return -1;
			}
			CloseHandle(h);
		}

		attr = data.dwFileAttributes;
		if(dir.mode & 0222)
			attr &= ~FILE_ATTRIBUTE_READONLY;
		else
			attr |= FILE_ATTRIBUTE_READONLY;
		if(attr != data.dwFileAttributes && (attr & FILE_ATTRIBUTE_READONLY))
			SetFileAttributes(path, attr);

		if(usesecurity
		&& secwperm(path, &dir, data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) < 0) {
fprint(2, "wstat: secwperm failed\n");
			return -1;
		}
		
		if(attr != data.dwFileAttributes && !(attr & FILE_ATTRIBUTE_READONLY))
			SetFileAttributes(path, attr);

	
		/* do last so path is valid throughout */
		p = wstrrchr(path, '\\');
		n = p-path;
		n++;
		memmove(newpath, path, n*sizeof(Rune));
		utftowstr(newpath+n, dir.name, MAX_PATH-n);
		if(wstrcmp(path, newpath) != 0 && !MoveFile(path, newpath)) {
			winerror();
fprint(2, "wstat: movefile failed %S %S: %r\n", path, newpath);
			return -1;
		}
		break;
	case Pserver:
		werrstr("cannot wstat to a volume or server");
		return -1;
	}

	return 0;
}

long
seek(int fd, long offset, int mode)
{
	File *f;
	HANDLE h;

	if(fd < 0 || fd >= Nfd) {
		werrstr("fd out of range");
		return -1;
	}

	f = &fdtbl[fd];
	qlock(&f->lk);
	switch(f->type) {
	default:
		fatal("unknown device type");
	case Tfree:
		qunlock(&f->lk);
		werrstr("invalid fd");
		return -1;
	case Tconsole:
	case Tstdin:
	case Tpipe:
		qunlock(&f->lk);
		werrstr("can not seek on device");
		offset = -1;
		break;
	case Tfile:
		h = f->h;
		qunlock(&f->lk);
		offset = SetFilePointer(h, offset, 0, mode);
		if(offset < 0) 
			winerror();
		break;
	case Tdir:
	case Tserver:
		switch(mode) {
		default:
			offset = -1;
			werrstr("bad mode for seek");
			break;
		case 0:
			f->offset = offset;
			break;
		case 1:
			f->offset += offset;
			break;
		case 2:
			f->offset = 0;
			break;
		}
		qunlock(&f->lk);
		break; 
	case Tdevnull:
		qunlock(&f->lk);
		break;
	}
	return offset;
}

int
dirdoread(File *f, uchar *buf, int n)
{
	int i;
	Rune path[MAX_PATH];
	WIN32_FIND_DATA data;
	n = (n/DIRLEN)*DIRLEN;
	if(n == 0)
		return 0;
	if(f->offset != f->cur || f->cur == 0) {
		f->cur = 0;
		wstrcpy(path, f->path);
		wstrncat(path, L"\\*.*", MAX_PATH);
		path[MAX_PATH-1] = 0;
		FindClose(f->h);
		f->h = FindFirstFile(path, &data);
		if(f->h == INVALID_HANDLE_VALUE)
			return 0;
		if(!dirbadentry(&data))	
			f->cur += DIRLEN;
		while(f->cur <= f->offset)	
			if(!dirnext(f, &data))
				return 0;
	} else if(!dirnext(f, &data))
		return 0;

	i=0;
	do {
		edirset(buf+i, &data, f->path, f->mode&ONOSEC);
		i += DIRLEN;
	} while(i < n && dirnext(f, &data));

	f->offset += i;

	return i;
}

int
dirbadentry(WIN32_FIND_DATA *data)
{
	Rune *s;

	s = data->cFileName;
	if(s[0] == 0)
		return 1;
	if(s[0] == '.' && (s[1] == 0 || s[1] == '.' && s[2] == 0))
		return 1;

	return 0;
}

int
dirnext(File *f, WIN32_FIND_DATA *data)
{
	for(;;) {
		if(!FindNextFile(f->h, data))
			return 0;
		if(!dirbadentry(data))
			break;
	}

	f->cur += DIRLEN;
	return 1;
}

HANDLE
serveropen(Rune *path)
{
	NETRESOURCE res;
	HANDLE h;

	res.dwScope = RESOURCE_GLOBALNET;
	res.dwType = RESOURCETYPE_DISK;
	res.dwDisplayType = RESOURCEDISPLAYTYPE_GENERIC;
	res.dwUsage = RESOURCEUSAGE_CONTAINER;
	res.lpLocalName = 0;
	res.lpRemoteName = path;
	res.lpComment = 0;
	res.lpProvider = 0;
	if (WNetOpenEnum(RESOURCE_GLOBALNET, RESOURCETYPE_DISK, 0,
			&res, &h) != NO_ERROR) {
		winerror();
		return INVALID_HANDLE_VALUE;
	}
	return h;
}

int
serverentry(File *f, Dir *dir)
{
	int count, n;
	uchar buf[1000];
	Rune *p;
	NETRESOURCE *res;


	n = sizeof(buf);
	count = 1;
	if(WNetEnumResource(f->h, &count, buf, &n) != NO_ERROR)
		return 0;
	res = (NETRESOURCE*)buf;
	memset(dir, 0, sizeof(Dir));
	p = wstrrchr(res->lpRemoteName, '\\');
	wstrtoutf(dir->name, p+1, nelem(dir->name));
	dir->mode = 0777 | CHDIR;
	dir->qid.path = CHDIR;
	f->cur += DIRLEN;
	return 1;
}

int
serverread(File *f, uchar *buf, int n)
{
	int i;
	Dir dir;
	int t;

	n = (n/DIRLEN)*DIRLEN;
	if(n == 0)
		return 0;

	t = time(0);

	if(f->offset != f->cur) {
		f->cur = 0;
		WNetCloseEnum(f->h);
		if((f->h = serveropen(f->path)) == INVALID_HANDLE_VALUE)
			return 0;
		while(f->cur < f->offset)	
			if(!serverentry(f, &dir))
				return 0;
	}


	for(i=0; i<n; i+= DIRLEN) {
		if(!serverentry(f, &dir))
			break;
		dir.mtime = t;
		dir.atime = t;
		dir.qid.vers = t;
		convD2M(&dir, buf+i);
	}

	f->offset += i;

	return i;
}

void
edirset(char *edir, WIN32_FIND_DATA *data, Rune *dpath, int nosec)
{
	Dir dir;
	Rune path[MAX_PATH];
	Rune *p;

	p = wstrecpy(path, dpath, path+MAX_PATH);
	p = wstrecpy(p, L"\\", path+MAX_PATH);
	p = wstrecpy(p, data->cFileName, path+MAX_PATH);

	wstrtoutf(dir.name, data->cFileName, nelem(dir.name));
	dir.qid.path = pathqid(0, path);
	dir.atime = unixtime(data->ftLastAccessTime);
	dir.mtime = unixtime(data->ftLastWriteTime);
	dir.qid.vers = dir.mtime;
	dir.length = data->nFileSizeLow;
	dir.hlength = data->nFileSizeHigh; 
	dir.type = 'w';
	dir.dev = 0;

	if(!usesecurity || nosec || secperm(&dir, path)<0){
		/* no NT security so make something up */
		strcpy(dir.uid, "none");
		strcpy(dir.gid, "none");
		dir.mode = 0666;
		p = wstrrchr(data->cFileName, '.');
		if(p != 0 && (wstrcmp(p, L".exe") == 0 || wstrcmp(p, L".rsh") == 0))
			dir.mode |= 0111;
		if(data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			dir.mode |= 0111;
	}

	if(data->dwFileAttributes & FILE_ATTRIBUTE_READONLY)
		dir.mode &= ~0222;
	if(data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
		dir.qid.path |= CHDIR;
		dir.mode |= CHDIR;
	}

	convD2M(&dir, edir);
}

File *
filealloc(void)
{
	int i;
	File *f;
	static int maxfd;

	for(i=0,f=fdtbl; i<Nfd; i++,f++) {
		if(i>maxfd) {
/*			wprint(L"maxfd = %d\n", i); */
			maxfd = i;
		}
		if(f->type != Tfree)
			continue;
		qlock(&f->lk);
		if(f->type == Tfree) {
			return f;
		}
		qunlock(&f->lk);
	}
	werrstr("too many file descriptors");
	return 0;
}

File *
filelookup(int fd)
{
	File *f;

	if(fd < 0 || fd >= Nfd) {
		werrstr("fd out of range");
		return 0;
	}
	
	f = &fdtbl[fd];
	return f;
}

int
setpath(Rune *path, char *file)
{
	Rune tmp[MAX_PATH+1], *last;
	Rune *p;
	int n;

	n = utftowstr(tmp, file, MAX_PATH);
	if(n >= MAX_PATH) {
		werrstr("file name too long");
		return Pbad;
	}

	for(p=tmp; *p; p++) {
		if(*p == '/')
			*p = '\\';
	}

	if(tmp[0] != 0 && tmp[1] == ':') {
		if(tmp[2] == 0) {
			tmp[2] = '\\';
			tmp[3] = 0;
		} else if(tmp[2] != '\\') {
			/* don't allow c:foo - only c:\foo */
			werrstr("illegal file name");
			return Pbad;
		}
	}

	path[0] = 0;
	n = GetFullPathName(tmp, MAX_PATH, path, &last);
	if(n >= MAX_PATH) {
		werrstr("file name too long");
		return Pbad;
	}

	if(n == 0 && tmp[0] == '\\' && tmp[1] == '\\' && tmp[2] != 0) {
		wstrcpy(path, tmp);
		return Pserver;
	}

	if(n == 0) {
		werrstr("bad file name");
		return Pbad;
	}

	for(p=path; *p; p++) {
		if(*p < 32 || *p == '*' || *p == '?') {
			werrstr("file not found");
			return Pbad;
		}
	}

	/* get rid of trailling \ */
	if(path[n-1] == '\\') {
		if(n <= 2) {
			werrstr("illegal file name");
			return Pbad;
		}
		path[n-1] = 0;
		n--;
	}

	if(path[1] == ':' && path[2] == 0) {
		path[2] = '\\';
		path[3] = '.';
		path[4] = 0;
		return Pvolume;
	}

	if(path[0] != '\\' || path[1] != '\\')
		return Pfile;

	for(p=path+2,n=0; *p; p++)
		if(*p == '\\')
			n++;
	if(n == 0)
		return Pserver;
	if(n == 1)
		return Pvolume;
	return Pfile;
}

int
pathqid(int oh, Rune *path)
{
	uint h;
	Rune *p;

	p = path;

	h = oh;
	h = (h*1000003 + '\\') & ~CHDIR;
	
	for(h=0; *p; p++)
		h = (h*1000003 + *p) & ~CHDIR;

	return h;
}

long
unixtime(FILETIME ft)
{
	vlong t;

	t = (vlong)ft.dwLowDateTime + ((vlong)ft.dwHighDateTime<<32);
	t -= (vlong)10000000*134774*24*60*60;

	return (long)(t/10000000);
}

FILETIME
wintime(ulong t)
{
	FILETIME ft;
	vlong vt;

	vt = (vlong)t*10000000+(vlong)10000000*134774*24*60*60;

	ft.dwLowDateTime = vt;
	ft.dwHighDateTime = vt>>32;

	return ft;
}

int
consolewrite(File *f, uchar *buf, int n)
{
	char *p;
	Rune buf2[1000], *q;
	int i, n2, on;
	static Lock lk;

	assert(holdqlock(&f->lk));

	if(f->h == INVALID_HANDLE_VALUE) {
		lock(&lk);
		if(!tmpconsole)
			tmpconsole = AllocConsole();
		unlock(&lk);
		f->h = GetStdHandle(STD_OUTPUT_HANDLE);
	}

	p = buf;
	on = n;

	/* handle bufial runes */
	if(f->nbuf) {
		i = f->nbuf;
		assert(i < UTFmax);
		while(i < UTFmax && n>0) {
			f->buf[i] = *p;
			i++;
			p++;
			n--;
			if(fullrune(f->buf, i)) {
				f->nbuf = 0;
				chartorune(buf2, f->buf);
				if(!WriteConsole(f->h, buf2, 1, &n, 0)) {
					winerror();
					return -1;
				}
				break;
			}
		}
	}

	while(n >= UTFmax || fullrune(p, n)) {
		n2 = nelem(buf2);
		q = buf2;

		while(n2) {
			if(n < UTFmax && !fullrune(p, n))
				break;
			i = chartorune(q, p);
			p += i;
			n -= i;
			n2--;
			q++;
		}
		
		if(!WriteConsole(f->h, buf2, q-buf2, &n2, 0)) {
			winerror();
			return -1;
		}
	}

	if(n != 0) {
		if(f->buf == 0)
			f->buf = mallocz(UTFmax);
		assert(n+f->nbuf < UTFmax);
		memcpy(f->buf+f->nbuf, p, n);
		f->nbuf += n;
	}

	return on;
}

/*
 * It appears that win NT 4.0 has a bug in ReadConsole
 * If ReadConsole is called with a buffer that is smaller than
 * the input that is buffered, it appears that readconsoles buffer
 * can get corrupted by WriteConsole
 * i.e
 *      char buf[3];
 *	int n2;
 *
 *	for(;;) {
 *		if(!ReadConsole(h, buf, sizeof(buf), &n2, 0))
 *			exit(0);
 *		if(!WriteConsole(h2, buf, n2, &n2, 0))
 *			exit(0);
 *
 *		Sleep(100);
 *	}
 *
 */
int
consoleread(File *f, uchar *buf, int n)
{
	int n2, i;
	Rune r, rbuf[200];
	char *p;

	assert(holdqlock(&f->lk));

	if(n == 0 || f->h == INVALID_HANDLE_VALUE)
		return 0;

	while(f->nbuf==0 && f->eof==0) {
		if(!ReadConsole(f->h, rbuf, nelem(rbuf), &n2, 0)) {
			winerror();
fprint(2, "readconsole error: %s\n", CT->error);
			return -1;
		}
		if(n2 == 0)
			continue;

		if(f->buf == 0)
			f->buf = mallocz(sizeof(rbuf)*UTFmax);
		
		for(i=0,p=f->buf; i<n2; i++) {
			r = rbuf[i];
			if(r == '\r')
				continue;
			if(r == 0x4) {
				f->eof = 1;
				break;
			}
			p += runetochar(p, &r);
		}
		f->nbuf = p-f->buf;
	}

	if(n > f->nbuf)
		n = f->nbuf;
	memmove(buf, f->buf, n);
	memmove(f->buf, f->buf+n, f->nbuf-n);
	f->nbuf -= n;
	return n;
}
