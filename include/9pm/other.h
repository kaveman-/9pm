/*
 * exception interface
#define	waserror()	(setjmp(PM_waserror()))

extern long	*PM_waserror(void);
extern void	nexterror(void);
extern void	error(char *fmt, ...);
extern void	poperror(void);
 */

/*
 * audio
 */
int	audioinit(void);
void	audioexit(void);
int	audiowrite(void*, int);
void	audiovolume(int);

/*
 * clibboard - tmp
 */
int	clipread(uchar*, int);
int	clipwrite(uchar*, int);
