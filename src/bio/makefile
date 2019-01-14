CC = cl
AR = lib
CFLAGS = -w -nologo -D_POSIX_ -DSYSV -I../include
O=obj
OS=obj

LIB=libbio.lib
OFILES=\
	bbuffered.$O\
	bfildes.$O\
	bflush.$O\
	bgetrune.$O\
	bgetc.$O\
	bgetd.$O\
	binit.$O\
	bprint.$O\
	bputrune.$O\
	bputc.$O\
	brdline.$O\
	bread.$O\
	bseek.$O\
	bwrite.$O\

HFILES= ..\include\bio.h

!include <..\mksyslib.mak>
