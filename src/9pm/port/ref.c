#include <u.h>
#include <9pm/libc.h>

int
refinc(Ref *r)
{
	int i;

	lock(&r->lk);
	i = r->ref;
	r->ref++;
	unlock(&r->lk);
	return i;
}

int	
refdec(Ref *r)
{
	int i;

	lock(&r->lk);
	r->ref--;
	i = r->ref;
	unlock(&r->lk);

	return i;
}
