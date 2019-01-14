#include "9pm.h"

long
utflen(char *s)
{
	int c;
	long n;
	Rune rune;

	n = 0;
	for(;;) {
		c = *(uchar*)s;
		if(c < Runeself) {
			if(c == 0)
				return n;
			s++;
		} else
			s += chartorune(&rune, s);
		n++;
	}
	return 0;
}

char*
utfrrune(char *s, Rune c)
{
	long c1;
	Rune r;
	char *s1;

	if(c < Runesync)		/* not part of utf sequence */
		return strrchr(s, c);

	s1 = 0;
	for(;;) {
		c1 = *(uchar*)s;
		if(c1 < Runeself) {	/* one byte rune */
			if(c1 == 0)
				return s1;
			if(c1 == c)
				s1 = s;
			s++;
			continue;
		}
		c1 = chartorune(&r, s);
		if(r == c)
			s1 = s;
		s += c1;
	}
	return 0;
}
char*
utfrune(char *s, Rune c)
{
	long c1;
	Rune r;
	int n;

	if(c < Runesync)		/* not part of utf sequence */
		return strchr(s, c);

	for(;;) {
		c1 = *(uchar*)s;
		if(c1 < Runeself) {	/* one byte rune */
			if(c1 == 0)
				return 0;
			if(c1 == c)
				return s;
			s++;
			continue;
		}
		n = chartorune(&r, s);
		if(r == c)
			return s;
		s += n;
	}
	return 0;
}

/*
 * Return pointer to first occurrence of s2 in s1,
 * 0 if none
 */
char*
utfutf(char *s1, char *s2)
{
	char *p;
	long n1, n2;
	Rune r;

	n1 = chartorune(&r, s2);
	if(r <= Runesync)		/* represents self */
		return strstr(s1, s2);

	n2 = strlen(s2);
	for(p=s1; p=utfrune(p, r); p+=n1)
		if(strncmp(p, s2, n2) == 0)
			return p;
	return 0;
}

int
utf2wstr(Rune *t, int n, char *s)
{
	int i;

	if(n <= 0)
		return utflen(s)+1;
	for(i=0; i<n; i++,t++) {
		if(*s == 0) {
			*t = 0;
			return i;
		}
		s += chartorune(t, s);
	}
	t[-1] = 0;
	return n + utflen(s)+1;
}

