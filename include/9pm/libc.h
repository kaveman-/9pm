#define	nelem(x)	(sizeof(x)/sizeof((x)[0]))
#define	assert(x)	if(!(x))PM_assert(0, __FILE__, __LINE__)

/*
 * mem routines
 */
extern	void*	memccpy(void*, void*, int, ulong);
extern	void*	memset(void*, int, ulong);
extern	int	memcmp(void*, void*, ulong);
extern	void*	memcpy(void*, void*, ulong);
extern	void*	memmove(void*, void*, ulong);
extern	void*	memchr(void*, int, ulong);

/*
 * string routines
 */
extern	char*	strcat(char*, char*);
extern	char*	strchr(char*, char);
extern	int	strcmp(char*, char*);
extern	char*	strcpy(char*, char*);
extern	char*	strdup(char*);
extern	char*	strncat(char*, char*, long);
extern	char*	strncpy(char*, char*, long);
extern	int	strncmp(char*, char*, long);
extern	char*	strpbrk(char*, char*);
extern	char*	strrchr(char*, char);
extern	char*	strtok(char*, char*);
extern	uint	strlen(const char*);
extern	long	strspn(char*, char*);
extern	long	strcspn(char*, char*);
extern	char*	strstr(char*, char*);
extern	char*	strecpy(char*, char*, char*);
extern	int	tokenize(char*, char**, int);

/*
 * wide string routines
 */
extern 	int	wstrutflen(Rune *s);
extern 	Rune	*wstrcpy(Rune*, Rune*);
extern 	Rune	*wstrncpy(Rune*, Rune*, int);
extern 	Rune	*wstrncat(Rune*, Rune*, int);
extern 	int	wstrncmp(Rune*, Rune*, int);
extern 	Rune	*wstrchr(Rune*, Rune);
extern 	Rune	*wstrrchr(Rune*, Rune);
extern 	Rune	*wstrcat(Rune*, Rune*);
extern 	int	wstrlen(Rune*);
extern 	Rune	*wstrdup(Rune*);
extern 	int	wstrcmp(Rune*, Rune*);
extern	Rune	*wstrecpy(Rune*, Rune*, Rune*);
extern 	int	wstrtoutf(char *s, Rune *t, int n);

enum
{
	UTFmax		= 3,		/* maximum bytes per rune */
	Runesync	= 0x80,		/* cannot represent part of a UTF sequence (<) */
	Runeself	= 0x80,		/* rune and UTF sequences are the same (<) */
	Runeerror	= 0x80,		/* decoding error in UTF */
};

/*
 * new rune routines
 */
extern	int	runetochar(char*, Rune*);
extern	int	chartorune(Rune*, char*);
extern	int	runelen(Rune);
extern	int	fullrune(char*, int);

/*
 * rune routines from converted str routines
 */
extern	long	utflen(char*);
extern	char*	utfrune(char*, Rune);
extern	char*	utfrrune(char*, Rune);
extern	char*	utfutf(char*, char*);
extern  int	utftowstr(Rune*, char*, int);

/*
 * malloc
 */
extern	void*	malloc(long);
extern	void*	mallocz(long);
extern	void	free(void*);
extern	void*	realloc(void*, long);
extern	void*	reallocz(void*, long);
extern	void	mallocstats(void);
extern	void*	sbrk(ulong);
extern	void	szero(void*, long);

/*
 * print routines
 */
typedef	struct	Fconv	Fconv;
struct	Fconv
{
	char*	out;		/* pointer to next output */
	char*	eout;		/* pointer to end */
	int	f1;
	int	f2;
	int	f3;
	int	chr;
};
extern	char*	doprint(char*, char*, char*, va_list);
extern	int	print(char*, ...);
extern	int	snprint(char*, int, char*, ...);
extern	int	sprint(char*, char*, ...);
extern	int	fprint(int, char*, ...);

extern	int	fmtinstall(int, int (*)(va_list*, Fconv*));
extern	int	numbconv(va_list*, Fconv*);
extern	void	strconv(char*, Fconv*);
extern	int	fltconv(va_list*, Fconv*);
/*
 * random number
 */
extern	void	srand(long);
extern	int	rand(void);
extern	int	nrand(int);
extern	long	lrand(void);
extern	long	lnrand(long);
extern	double	frand(void);

/*
 * math
 */
extern	ulong	getfcr(void);
extern	void	setfsr(ulong);
extern	ulong	getfsr(void);
extern	void	setfcr(ulong);
extern	double	NaN(void);
extern	double	Inf(int);
extern	int	isNaN(double);
extern	int	isInf(double, int);

extern	double	pow(double, double);
extern	double	atan2(double, double);
extern	double	fabs(double);
extern	double	atan(double);
extern	double	log(double);
extern	double	log10(double);
extern	double	exp(double);
extern	double	floor(double);
extern	double	ceil(double);
extern	double	hypot(double, double);
extern	double	sin(double);
extern	double	cos(double);
extern	double	tan(double);
extern	double	asin(double);
extern	double	acos(double);
extern	double	sinh(double);
extern	double	cosh(double);
extern	double	tanh(double);
extern	double	sqrt(double);
extern	double	fmod(double, double);

#define	HUGE	3.4028234e38
#define	PIO2	1.570796326794896619231e0
#define	PI	(PIO2+PIO2)

/*
 * Time-of-day
 */

typedef
struct Tm
{
	int	sec;
	int	min;
	int	hour;
	int	mday;
	int	mon;
	int	year;
	int	wday;
	int	yday;
	char	zone[4];
	int	tzoff;
} Tm;

extern	Tm*	gmtime(long);
extern	Tm*	localtime(long);
extern	char*	asctime(Tm*);
extern	char*	ctime(long);
extern	long	time(long*);

/*
 * one of a kind
 */
extern	int	abs(int);
extern	double	atof(char*);
extern	int	atoi(char*);
extern	long	atol(char*);
extern	double	charstod(int(*)(void*), void*);
extern	char	*cleanname(char*);
extern	int	decrypt(void*, void*, int);
extern	int	encrypt(void*, void*, int);
extern	double	frexp(double, int*);
extern	char	*getenv(char*);
extern	char	**getenvall(void);
extern	int	getfields(char*, char**, int);
extern	int	getmfields(char*, char**, int);
extern	char*	getuser(void);
extern	char*	getwd(char*, int);
extern	long	labs(long);
extern	double	ldexp(double, int);
extern	char	*mktemp(char*);
extern	double	modf(double, double*);
extern	int	netcrypt(void*, void*);
extern	double	pow10(int);
extern	int	putenv(char*, char*);
extern	void	qsort(void*, long, long, int (*)(void*, void*));
extern	char*	setfields(char*);
extern	double	strtod(char*, char**);
extern	long	strtol(char*, char**, int);
extern	ulong	strtoul(char*, char**, int);
extern	void	sysnop(void);
extern	int	tolower(int);
extern	int	toupper(int);

/*
 * pack/unpack
 */
#define	PKUSHORT(p, v)		((p)[0]=(v), (p)[1]=((v)>>8))
#define	PKLONG(p, v)		(PKUSHORT(p, (v)), PKUSHORT(p+2, (v)>>16))
#define	PKULONG(p, v)		(PKUSHORT(p, (v)), PKUSHORT(p+2, (v)>>16))
#define	UNPKUSHORT(p)		(((p)[0]<<0) | ((p)[1]<<8))
#define	UNPKLONG(p)		((UNPKUSHORT(p)<<0) | (UNPKUSHORT(p+2)<<16))
#define	UNPKULONG(p)		((UNPKUSHORT(p)<<0) | (UNPKUSHORT(p+2)<<16))

extern	void	pklong(uchar*, long);
extern  void	pkvlong(uchar*, vlong);
extern  void	pkushort(uchar*, uint);
extern	void	pkulong(uchar*, ulong);
extern  void	pkuvlong(uchar*, uvlong);
extern  void	pkfloat(uchar*, float);
extern  void	pkdouble(uchar*, double);

extern	long	unpklong(uchar*);
extern  vlong	unpkvlong(uchar*);
extern  uint	unpkushort(uchar*);
extern	ulong	unpkulong(uchar*);
extern  uvlong	unpkuvlong(uchar*);
extern  float	unpkfloat(uchar*);
extern  double	unpkdouble(uchar*);

/*
 * io
 */

typedef struct Qid	Qid;
typedef struct Dir	Dir;

#define	MAXFDATA (8*1024) 	/* max length of read/write Blocks */
#define	NAMELEN	64		/* length of path element, including '\0' */
#define	MAXPATH	250		/* maximum file path name, including '\0' */
#define	DIRLEN	(NAMELEN*3+5*4+8+2+2)	/* length of machine-independent Dir structure */
#define	ERRLEN	64		/* length of error string */

enum {
	OREAD	= 0,		/* open for read */
	OWRITE	= 1,		/* write */
	ORDWR	= 2,		/* read and write */
	OEXEC	= 3,		/* execute, == read but check execute permission */
	OTRUNC	= (1<<4),	/* or'ed in (except for exec), truncate file first */
	OCEXEC	= (1<<5),	/* or'ed in, close on exec - ignored */
	ORCLOSE	= (1<<6),	/* or'ed in, remove on close */
	ONOSEC	= (1<<7),	/* or'ed in, no securtiy info - optimization for ls, etc */
};


#define CHDIR		0x80000000	/* mode bit for directories */
#define CHAPPEND	0x40000000	/* mode bit for append only files */
#define CHEXCL		0x20000000	/* mode bit for exclusive use files */
#define CHMOUNT		0x10000000	/* mode bit for mounted channel */
#define CHREAD		0x4		/* mode bit for read permission */
#define CHWRITE		0x2		/* mode bit for write permission */
#define CHEXEC		0x1		/* mode bit for execute permission */


struct Qid
{
	ulong	path;
	ulong	vers;
};

struct Dir
{
	char	name[NAMELEN];
	char	uid[NAMELEN];
	char	gid[NAMELEN];
	Qid	qid;
	ulong	mode;
	int	atime;
	int	mtime;
	Length;
	ushort	type;
	ushort	dev;
};

extern	int	access(char*, int);
extern	int	chdir(char*);
extern	int	close(int);
extern	int	create(char*, int, ulong);
extern	int	dup(int, int);
extern  int	export(int);
extern	int	fauth(int, char*);
extern	int	fsession(int, char*);
extern	int	fstat(int, char*);
extern	int	fwstat(int, char*);
extern	int	open(char*, int);
extern	int	pipe(int*);
extern	int	pipeeof(int*);
extern	long	read(int, void*, long);
extern	long	readn(int, void*, long);
extern	long	readx(int, void*, long, long);
extern	int	remove(char*);
extern	long	seek(int, long, int);
extern	int	stat(char*, char*);
extern	long	write(int, void*, long);
extern	long	writex(int, void*, long, long);
extern	int	wstat(char*, char*);

extern  int	convM2D(char*, Dir*);
extern  int	convD2M(Dir*, char*);
extern	int	dirstat(char*, Dir*);
extern	int	dirfstat(int, Dir*);
extern	int	dirwstat(char*, Dir*);
extern	long	dirread(int, Dir*, long);

extern	int	errstr(char*);
extern	void	werrstr(char*, ...);
extern	void	perror(char*);
extern	void	syslog(int, char*, char*, ...);


/*
 * processes and threads
 */
extern  uint	proc(char**, int, int, int, char**);
extern  int	procwait(uint, double[3]);
extern	int	prockill(uint);
extern	uint	getpid(void);
extern	void	_exits(char*);
extern	int	atexit(void(*)(void));
extern	void	atexitdont(void(*)(void));
extern	void	exits(char*);
extern  void	fatal(char*, ...);
extern	void	abort(void);

extern  uint	thread(void(*f)(void*), void*);
extern	void	threadexit(void);
extern  void	threadnowait(void);
extern	double	threadtime(void);
extern	uint	gettid(void);
extern	int	gettindex(void);

extern	int	sleep(long);
extern	long	times(long*);
extern	double	cputime(void);
extern	double	realtime(void);

extern  int	consintr(void);
extern	int	consonintr(void (*f)(void));

extern	void	longjmp(jmp_buf, int);
extern	int	setjmp(jmp_buf);

typedef struct Lock	Lock;
typedef struct Qlock	Qlock;
typedef struct RWlock	RWlock;
typedef struct Event	Event;
typedef struct Ref	Ref;

struct Lock {
	int	val;
};

struct Qlock {
	Lock	lk;
	void	*hold;
	int	nwait;
};

struct RWlock {
	Lock	lk;
	void	*hold;
	int	readers;
	int	nwait;
	uint	hash;
};

struct Ref
{
	Lock	lk;
	int	ref;
};

struct Event
{
	Lock	lk;
	int	nwait;
	int	ready;
	int	poison;
};

extern int	tas(int*);

extern void	lock(Lock*);
extern void	unlock(Lock*);
extern int	canlock(Lock*);
extern void	lockwait(Lock*);
extern int	locksignal(Lock*);
extern int	locksignalall(Lock*);

extern void	qlock(Qlock*);
extern void	qunlock(Qlock*);
extern int	canqlock(Qlock*);
extern int	holdqlock(Qlock*);

extern void	rlock(RWlock*);
extern void	runlock(RWlock*);
extern int	canrlock(RWlock*);
extern int	holdrlock(RWlock*);
extern void	wlock(RWlock*);
extern void	wunlock(RWlock*);
extern int	canwlock(RWlock*);
extern int	holdwlock(RWlock*);
extern int	holdrwlock(RWlock*);

extern void	esignal(Event*);
extern void	ereset(Event*);
extern void	epoison(Event*);
extern int	ewait(Event*);
extern int	epoll(Event*);

extern int	refinc(Ref*);
extern int	refdec(Ref*);

/* 
 * for debugging
 */
extern ulong	getcallerpc(void*);
extern int	PM_assert(int,char*,int);

extern char *argv0;
#define	ARGBEGIN	for((argv0? 0: (argv0=*argv)),argv++,argc--;\
			    argv[0] && argv[0][0]=='-' && argv[0][1];\
			    argc--, argv++) {\
				char *_args, *_argt;\
				Rune _argc;\
				_args = &argv[0][1];\
				if(_args[0]=='-' && _args[1]==0){\
					argc--; argv++; break;\
				}\
				_argc = 0;\
				while(*_args && (_args += chartorune(&_argc, _args)))\
				switch(_argc)
#define	ARGEND		SET(_argt);USED(_argt); USED(_argc); USED(_args);}USED(argv); USED(argc);
#define	ARGF()		(_argt=_args, _args="",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): 0))
#define	ARGC()		_argc
