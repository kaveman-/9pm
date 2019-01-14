#pragma comment( lib,"c.lib" )
#pragma comment( lib,"9pm2.lib")

typedef	unsigned short	ushort;
typedef	unsigned char	uchar;
typedef unsigned long	ulong;
typedef unsigned int	uint;
typedef signed char	schar;
typedef	__int64		vlong;
typedef	unsigned __int64 uvlong;
typedef	ushort		Rune;
typedef union
{
	char	clength[8];
	vlong	vlength;
	struct
	{
		long	hlength;
		long	length;
	};
} Length;
#define tls		__declspec( thread )


/* FCR */
#define	FPINEX	(1<<5)
#define	FPOVFL	(1<<3)
#define	FPUNFL	((1<<4)|(1<<1))
#define	FPZDIV	(1<<2)
#define	FPRNR	(0<<10)
#define	FPRZ	(3<<10)
#define	FPRPINF	(2<<10)
#define	FPRNINF	(1<<10)
#define	FPRMASK	(3<<10)
#define	FPPEXT	(3<<8)
#define	FPPSGL	(0<<8)
#define	FPPDBL	(2<<8)
#define	FPPMASK	(3<<8)
/* FSR */
#define	FPAINEX	FPINEX
#define	FPAOVFL	FPOVFL
#define	FPAUNFL	FPUNFL
#define	FPAZDIV	FPZDIV

#define	USED(x)		if(x);else
#define	SET(x)

typedef char *	va_list;

#define _INTSIZEOF(n)	( (sizeof(n) + sizeof(int) - 1) & ~(sizeof(int) - 1) )

#define va_start(ap,v)	( ap = (va_list)&v + _INTSIZEOF(v) )
#define va_arg(ap,t)	( *(t *)((ap += _INTSIZEOF(t)) - _INTSIZEOF(t)) )
#define va_end(ap)	( ap = (va_list)0 )

/*
 * compiler only noties _setjmp
 *	disables registeration in function - so local variables are in the
 *      same state as when longjmp was called
 */
#define	setjmp		_setjmp
typedef long		jmp_buf[16];

/* remove const problems */
#define const

/* disable various silly warnings */
#pragma warning( disable : 4018 4305 4244 4102 4761 4164 4769 )

/* disable intrinsics for some functions */
#pragma function ( inp, outp, inpw, outpw )
#pragma function ( acos, asin, cosh, fmod, pow, sinh, tanh )
#pragma function ( memcpy, memset, memcmp, strcat, strcmp, strcpy, strlen )
/* #pragma funciton ( abs labs fabs ) */
/* #pragma function ( atan, atan, cos, exp, log, log10, sin, sqrt, tan ) */
