#include <u.h>
#include <9pm/libc.h>
#include "9pm.h"

typedef struct Tag Tag;

enum
{
	NHLOG	= 7,
	NHASH	= (1<<NHLOG)
};

struct Tag
{
	void*	tag;
	Thread	*first;
	Thread	*last;
	Tag	*next;
};

static	Tag*	ht[NHASH];
static	Tag*	ft;
static	Lock	hlock;

void
threadqueue(void *tag)
{
	uint h;
	Tag *t;
	Thread *ct;

	ct = CT;
	ct->qnext = 0;

	h = ((ulong)tag>>2) & (NHASH-1);

	lock(&hlock);
	for(t = ht[h]; t; t = t->next) {
		if(t->tag == tag) {
			t->last->qnext = ct;
			t->last = ct;	
			unlock(&hlock);
			return;
		}
	}

	t = ft;
	if(t == 0)
		t = sbrk(sizeof(Tag));
	else
		ft = t->next;

	t->tag = tag;
	t->first = ct;
	t->last = ct;
	t->next = ht[h];
	ht[h] = t;

	unlock(&hlock);
}

Thread *
threaddequeue(void *tag)
{
	uint h;
	Tag *t, **l;
	Thread *rt;

	h = ((ulong)tag>>2) & (NHASH-1);

	lock(&hlock);
	l = &ht[h];
	for(t = ht[h]; t; l=&t->next,t = t->next) {
		if(t->tag == tag) {
			rt = t->first;
			if(rt->qnext == 0) {
				*l = t->next;
				t->next = ft;
				ft = t;
			} else
				t->first = rt->qnext;
			unlock(&hlock);
			return rt;
		}
	}

	unlock(&hlock);
	return 0;
}
