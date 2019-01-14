#define SDB	if(debug&0x2)fprint(2,
#define	EDB	);

enum	/* constants for I/O to devgraphics */
{
	Tilehdr = 40,
	Tilesize = 8000
};

enum
{
	EMAXMSG = 128+8192,	/* size of 9p header+data */
};

/*
 * Types
 */

typedef struct Point		Point;
typedef struct Rectangle 	Rectangle;
typedef struct Fid		Fid;
typedef struct Dirtab		Dirtab;
typedef struct Bitblt		Bitblt;
typedef struct Bitmap		Bitmap;
typedef struct Fontchar		Fontchar;
typedef struct Subfont		Subfont;
typedef struct Glyph		Glyph;
typedef struct Linedesc		Linedesc;

enum
{
	Qdir,
	Qbitblt,
	Qbitbltstats,
};

/*
 * return types for RPCs on /dev/bitblt 
 */
enum
{
	Rnone,
	Rbitmap,
	Rsubf,
	Rcache,
	Rfont,
	Runload,
};

struct	Point
{
	int	x;
	int	y;
};

struct Rectangle
{
	Point min;
	Point max;
};

struct Bitblt
{
	int		type;
	int		value;
	Rectangle	vrect;
};

struct Fid
{
	int		busy;
	int		open;
	int		fid;
	Qid		qid;
	Dirtab		*dir;
	union {
		Bitblt;			/* bitblt */
		char	*stats;		/* bitblt stats */
	};
	Fid		*next;
};


struct Dirtab
{
	char	*name;
	uint	qid;
	uint	perm;
};


struct	Bitmap
{
	Rectangle	r;		/* rectangle in data area, local coords */
	Rectangle 	clipr;		/* clipping region */
	int		ldepth;		/* log base 2 of number of bits per pixel */
	Fid		*f;		/* fid that owns this bitmap */

	int		wid;		/* when attached */
	Rectangle	bb;		/* bounding box of changes */
	int		waste;		/* amount of wasted space in bb */

	ulong		*base;		/* pointer to start of data */
	int		zero;		/* base+zero=&word containing (0,0) */
	ulong		width;		/* width in words of total data area */
};

struct	Fontchar
{
	short	x;		/* left edge of bits */
	uchar	top;		/* first non-zero scan-line */
	uchar	bottom;		/* last non-zero scan-line + 1 */
	char	left;		/* offset of baseline */
	uchar	width;		/* width of baseline */
};

struct	Subfont
{
	int		ref;
	uint		hash;
	uint		qid;		/* unique number - for glyph cache */
	int		n;		/* number of chars in font */
	int		height;		/* height of bitmap */
	int		ascent;		/* top of bitmap to baseline */
	Fontchar	*info;		/* n+1 character descriptors */
	Bitmap		*bits;		/* of font */
};

struct Glyph
{
	Subfont	*f;
	int	off;	/* offset with subfont */
};

struct Linedesc
{
	int	x0;
	int	y0;
	char	xmajor;
	char	slopeneg;
	long	dminor;
	long	dmajor;
};

/*
 * Codes for bitblt etc.
 *
 *	       D
 *	     0   1
 *         ---------
 *	 0 | 1 | 2 |
 *     S   |---|---|
 * 	 1 | 4 | 8 |
 *         ---------
 *
 *	Usually used as D|S; DorS is so tracebacks are readable.
 */
typedef
enum	Fcode
{
	Zero		= 0x0,
	DnorS		= 0x1,
	DandnotS	= 0x2,
	notS		= 0x3,
	notDandS	= 0x4,
	notD		= 0x5,
	DxorS		= 0x6,
	DnandS		= 0x7,
	DandS		= 0x8,
	DxnorS		= 0x9,
	D		= 0xA,
	DornotS		= 0xB,
	S		= 0xC,
	notDorS		= 0xD,
	DorS		= 0xE,
	F		= 0xF,
} Fcode;

/* arith.c */
Point		ptadd(Point, Point);
Point		ptsub(Point, Point);
Point		ptmul(Point, int);
Point		ptdiv(Point, int);
int		pteq(Point, Point);
int		ptinrect(Point, Rectangle);
Rectangle 	rectsubpt(Rectangle, Point);
Rectangle	rectaddpt(Rectangle, Point);
Rectangle	rectinset(Rectangle, int);
Rectangle	rectmul(Rectangle, int);
Rectangle	rectdiv(Rectangle, int);
Rectangle	rectshift(Rectangle, int);
Rectangle	rectcanon(Rectangle);
int		rectinrect(Rectangle, Rectangle);
int		rectXrect(Rectangle, Rectangle);
int		recteq(Rectangle, Rectangle);
int		rectclip(Rectangle*, Rectangle);

void		binit(void);
Bitmap*	 	balloc(Rectangle, int);
void	 	bfree(Bitmap*);
int		linesize(Bitmap*, Rectangle);
ulong		bunload(Bitmap *i, Rectangle r, uchar *data);
void		bload(Bitmap *i, Rectangle r, uchar *data);
void		bflush(Bitmap *b);
int		bshow(Bitmap *ba, int wid, Rectangle r);
void		bshade(int wid, Rectangle r);
void		bbound(Bitmap *b, Rectangle);
int		setclipr(Bitmap *b, Rectangle r);

void		bltinit(void);
void	 	bitblt(Bitmap*, Point, Bitmap*, Rectangle, Fcode);
int	 	bitbltclip(void*);
void		texture(Bitmap*, Rectangle, Bitmap*, Fcode);
ulong		*wordaddr(Bitmap*, Point);
uchar		*byteaddr(Bitmap*, Point);
int		clipline(Rectangle, Point*, Point*, Linedesc*);
void		segment(Bitmap*, Point, Point, int, Fcode);
void	 	point(Bitmap*, Point, int, Fcode);
void		glyph(Bitmap*, Point, int, int, Glyph, int);

void		fs(int, int);
void		usage();
void		dostat(Dirtab *dir, char *buf, uint clock);
void		notifyf(void *, char *s);

void		bitinit(void);
char 		*bitwrite(Fid *f, uchar *p, int n);
void		bitclose(Fid *f);
char		*bitstats(void);

uint		getclock();
void		reshape();

Bitmap		*getbitmap(int i, Fid *f);

extern Fid	*fids;
extern int	sfd, cfd;
extern int	clockfd;
extern char	user[NAMELEN];
extern int	debug;

extern char	Eperm[];
extern char	Enotdir[];
extern char	Enotexist[];
extern char	Einuse[];
extern char	Eexist[];
extern char	Eisopen[];
extern char	Excl[];
extern char	Ename[];
extern char	Ebadreq[];
extern char	Ebadbitmap[];
extern char	Ebadsubf[];

#define	IPSHORT(p, v)		{(p)[0]=(v); (p)[1]=((v)>>8);}
#define	IPLONG(p, v)		{IPSHORT(p, (v)); IPSHORT(p+2, (v)>>16);}
#define	IGSHORT(p)		(((p)[0]<<0) | ((p)[1]<<8))
#define	IGLONG(p)		((IGSHORT(p)<<0) | (IGSHORT(p+2)<<16))

extern Point	Pt(int, int);
extern Rectangle Rect(int, int, int, int);
extern Rectangle Rpt(Point, Point);

#define	Dx(r)	((r).max.x-(r).min.x)
#define	Dy(r)	((r).max.y-(r).min.y)
