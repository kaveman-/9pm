#define UNICODE
#define _WIN32_WINNT 0x0400
#include <windows.h>

/* disable various silly warnings */
#pragma warning( disable : 4305 4244 4102 4761)

#define	nelem(x)	(sizeof(x)/sizeof((x)[0]))

#define assert(x)	if(!(x)) win_fatal("assert failed");

typedef unsigned char	uchar;
typedef unsigned short	Rune;

/* utf constants */
enum
{
	UTFmax		= 3,		/* maximum bytes per rune */
	Runesync	= 0x80,		/* cannot represent part of a UTF sequence (<) */
	Runeself	= 0x80,		/* rune and UTF sequences are the same (<) */
	Runeerror	= 0x80,		/* decoding error in UTF */
};

extern	int	win_runetochar(char*, Rune*);
extern	int	win_chartorune(Rune*, char*);
extern	int	win_runelen(Rune);
extern	int	win_fullrune(char*, int);

/*
 * rune routines from converted str routines
 */
extern	long	win_utflen(char*);
extern	char*	win_utfrune(char*, Rune);
extern	char*	win_utfrrune(char*, Rune);
extern	char*	win_utfutf(char*, char*);
extern  int	win_wstrlen(Rune*);

extern 	int	win_utf2wstr(Rune*, int n, char*);
extern  int	win_wstr2utf(char*, int n, Rune*);

extern  void	win_fatal(char*);

extern	char	*win_error(char*, int);
extern	int	win_hasunicode(void);

extern	void	win_convfiledata(WIN32_FIND_DATAW *fdw, WIN32_FIND_DATAA *fda);

extern	int win_useunicode;
