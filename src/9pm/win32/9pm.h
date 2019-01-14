
typedef struct Thread	Thread;
typedef struct File	File;

enum {
	Nfd	= 100,	/* maximum number of open fds */
	Nrlook	= 5,	/* hash look for rwlocks */
	Nargs	= 1000, /* maximum number of cmd line arguments */
};

/* various platforms */
enum {
	Win95,
	WinNT,
};


/* file descriptor types */
enum {
	Tfree,		/* not used */
	Tfile,		/* a regular file */
	Tpipe,		/* pipe */
	Tstdin,		/* pipe acting as an interactive stdin - notices <crl>d */
	Tconsole,	/* windows input console */
	Tdir,		/* a directory */
	Tserver,	/* a server i.e. file of the form //server */
	Tdevnull,	/* dev null */
};

/* path types */
enum {
	Pbad,
	Pfile,
	Pvolume,
	Pserver,
};

struct Thread {
	int	tid;
	int	tindex;

	int	nowait;

	Thread	*qnext;		/* for qlock */

	/* rlock table */
	int	nrtab;		/* must be a power of two */
	RWlock	**rtab;

	char	error[ERRLEN];

	void	(*f)(void *);
	void	*a;

	void	*sema;		/* local handle to semaphore */
};


struct File {
	Qlock	lk;
	int	type;
	/* the following is zeroed when a File is allocated */
	Rune	*path;
	int	qid;
	int	mode;		/* mode that file was opened for */
	int	eof;		/* reached eof - but has not closed */
	int	offset;		/* offset within file */
	void	*h;		/* handle for object */
	union {
		int	cur;
		struct {		/* for consoles */
			int nbuf;
			char *buf;
		};
	};
};

/* env.c */
void	envinit(void);
Rune	*getenvblock(void);

/* fd.c */
void	fdinit(void);
int	setpath(Rune*, char*);
File	*filelookup(int fd);

/* queue.c*/
extern	void	threadqueue(void*);
extern	Thread	*threaddequeue(void*);
extern	void	threadsleep(void);
extern	void	threadwakeup(Thread*);

/* proc.c */
extern  void	procinit(void);

/* thread.c */
void	threadinit(void);
void	winerror(void);

void	timezoneinit(void);

/* sec.c*/
void	secinit(void);
int	secperm(Dir *d, Rune *path);
int	secwperm(Rune *path, Dir *d, int);


extern int	pm_assert(int);

extern	_declspec(thread) Thread	*CT;
extern	int	useunicode, usesecurity;
extern	int	tmpconsole;
extern	char	*pm_root;
extern	int	consintrbug;