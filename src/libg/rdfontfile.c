#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

static char*
skip(char *s)
{
	while(*s==' ' || *s=='\n' || *s=='\t')
		s++;
	return s;
}

Font*
rdfontfile(char *name)
{
	Font *fnt;
	Fontmap *c;
	int fd, i;
	char *buf, *s, *t;
	Dir dir;
	ulong min, max;
	int offset;

	fd = open(name, OREAD);
	if(fd < 0)
		return 0;
	if(dirfstat(fd, &dir) < 0){
    Err0:
		close(fd);
		return 0;
	}
	buf = malloc(dir.length+1);
	if(buf == 0)
		goto Err0;
	buf[dir.length] = 0;
	i = read(fd, buf, dir.length);
	close(fd);
	if(i != dir.length){
    Err1:
		free(buf);
		return 0;
	}

	s = buf;
	fnt = malloc(sizeof(Font));
	if(fnt == 0)
		goto Err1;
	memset(fnt, 0, sizeof(Font));
	fnt->name = strdup(name);
	if(fnt->name==0){
    Err2:
		free(fnt->name);
		free(fnt->map);
		free(fnt);
		goto Err1;
	}
	fnt->height = strtol(s, &s, 0);
	s = skip(s);
	fnt->ascent = strtol(s, &s, 0);
	s = skip(s);
	if(fnt->height<=0 || fnt->ascent<=0)
		goto Err2;
	fnt->nmap = 0;

	do{
		min = strtol(s, &s, 0);
		s = skip(s);
		max = strtol(s, &s, 0);
		s = skip(s);
		if(*s==0 || min>=65536 || max>=65536 || min>max){
    Err3:
			ffree(fnt);
			return 0;
		}
		t = s;
		offset = strtol(s, &t, 0);
		if(t>s && (*t==' ' || *t=='\t' || *t=='\n'))
			s = skip(t);
		else
			offset = 0;
		fnt->map = realloc(fnt->map, (fnt->nmap+1)*sizeof(Fontmap*));
		if(fnt->map == 0){
			/* realloc manual says fnt->sub may have been destroyed */
			fnt->nmap = 0;
			goto Err3;
		}
		c = malloc(sizeof(Fontmap));
		memset(c, 0, sizeof(Fontmap));
		if(c == 0)
			goto Err3;
		fnt->map[fnt->nmap] = c;
		c->min = min;
		c->max = max;
		c->offset = offset;
		t = s;
		while(*s && *s!=' ' && *s!='\n' && *s!='\t')
			s++;
		*s++ = 0;
		c->name = strdup(t);
		if(c->name == 0){
			free(c);
			goto Err3;
		}
		s = skip(s);
		fnt->nmap++;
	}while(*s);
	free(buf);
	return fnt;
}

void
ffree(Font *f)
{
	USED(f);
}
