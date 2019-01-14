#include	"mk.h"

int	initdone = 0;

/*
 *	Assemble a line skipping blank lines, comments, and eliding
 *	escaped newlines
 */
int
assline(Biobuf *bp, Bufblock *buf)
{
	int c;
	int lastc;

	initdone = 0;
	buf->current=buf->start;
	while ((c = nextrune(bp, 1)) >= 0){
		switch(c)
		{
		case '\n':
			if (buf->current != buf->start) {
				insert(buf, 0);
				return 1;
			}
			break;		/* skip empty lines */
		case '\\':
		case '\'':
		case '"':
			rinsert(buf, c);
			if (escapetoken(bp, buf, 1, c) == 0)
				Exit();
			break;
		case '`':
			if (bquote(bp, buf) == 0)
				Exit();
			break;
		case '#':
			lastc = '#';
			while ((c = Bgetc(bp)) != '\n') {
				if (c < 0)
					goto eof;
				lastc = c;
			}
			mkinline++;
			if (lastc == '\\')
				break;		/* propagate escaped newlines??*/
			if (buf->current != buf->start) {
				insert(buf, 0);
				return 1;
			}
			break;
		default:
			rinsert(buf, c);
			break;
		}
	}
eof:
	insert(buf, 0);
	return *buf->start != 0;
}
/*
 *	get next character stripping escaped newlines
 *	the flag specifies whether escaped newlines are to be elided or
 *	replaced with a blank.
 */
int
nextrune(Biobuf *bp, int elide)
{
	int c;

	for (;;) {
		c = Bgetrune(bp);
		if (c == '\\') {
			if (Bgetrune(bp) == '\n') {
				mkinline++;
				if (elide)
					continue;
				return ' ';
			}
			Bungetrune(bp);
		}
		if (c == '\n')
			mkinline++;
		return c;
	}
	return 0;
}
