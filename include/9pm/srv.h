#pragma comment ( lib, "srv.lib" )
/*
 * sever inferface 
 */
typedef struct Sinfo	Sinfo;
typedef struct Stype	Stype;

enum {
	Smesgsize	= 8000,		/* minimum size limit for rpc mesgs */
	Snamelen	= 28,		/* maximum length including null for a server name */
	Spathlen	= 10*Snamelen,	/* maximum path length for a server name */
	Sinfolen	= 16+2*Snamelen+4,
	Sidlen		= 4,		/* in ints */
};

/* flags in Singo */
enum {
	Sdir		= (1<<0),	/* is a directory */
};

struct Sinfo
{
	int	id[Sidlen];
	char	name[Snamelen];
	char	type[Snamelen];
	int	flags;
	int	mesgsize;
};
	
struct Stype
{
	void	*(*init)(void*, Sinfo*);
	int	(*fini)(void*);
	void	*(*open)(void*, int, char*, char*, Sinfo*);
	int	(*close)(void*, void*);
	int	(*free)(void*, void*);
	int	(*read)(void*, void*, uchar*, int);
	int	(*write)(void*, void*, uchar*, int);
	int	(*rpc)(void*, void*, uchar*, int, int);
	int	(*callback)(void*, void*, void (*f)(int, uchar*, int));
};	

extern void	srvinit(void);
extern void	srvfini(void);
extern int	srvmount(char*, Stype*, void*);
extern int	srvunmount(char*);
extern void	srvproxy(int);
extern int	srvopen(char*, char*);
extern int	srvinfo(int, Sinfo*);
extern int	srvpath(int, char*);
extern int	srvclose(int);
extern int	srvread(int, void*, int);
extern int	srvwrite(int, void*, int);
extern int	srvrpc(int, void*, int, int);
extern int	srvcallback(int, void (*f)(int, uchar*, int));

extern void	srvgenid(int*, void*);

extern void	pksinfo(uchar*, Sinfo*);
extern void	unpksinfo(Sinfo*, uchar*);	

/* some standard servers */
extern	Stype	srvremote;
extern	Stype	srvping;
extern  Stype	srvtcp;
extern  Stype	srvudp;
extern  Stype	srvpipe;
extern  Stype	srvnamedpipe;
extern  Stype	srvsmc;

/*
 *  network dialing and authentication
 */
extern	int	accept(int);
extern	int	announce(char*, char*);
extern	int	dial(char*, char*, char*);
extern	int	hangup(int);
extern	int	listen(int, char*);
extern	void	netmkaddr(char*, int, char*, char*, char*);
extern	int	reject(int, char*);
extern	int	netsrvset(char*, char*);
extern	char	*netsrvget(char);
