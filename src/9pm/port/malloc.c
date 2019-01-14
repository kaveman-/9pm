#include <u.h>
#include <9pm/libc.h>

typedef struct Bucket	Bucket;
typedef struct Info	Info;

enum
{
	MAGIC		= 0xbada110c,
	MAX2SIZE	= 32,
	CUTOFF		= 12,
	Hash		= 1009,
};

struct Bucket
{
	Info	*info;
	union {
		Bucket	*next;
		int	size;
	};
	char	data[1];
};

struct Info
{	
	int	magic;
	ulong	pc;
	int	n;
	int	size;
	Info	*next;
};

static int	bucket(int);
static void	*domalloc(long, ulong, int);
static void	*dorealloc(void*, long, ulong, int);
static Info	*lookup(ulong);

static Bucket	*arena[MAX2SIZE];
static Info	*hash[Hash];
static Lock	mlock;
static Info	*mallocinfo;

#define HDRSIZE		((int)((Bucket*)0)->data)

static int
bucket(int size)
{
	int pow;

	assert(size >= 0);

	for(pow = 3; pow < MAX2SIZE; pow++) {
		if(size <= (1<<pow))
			break;
	}

	assert(pow < MAX2SIZE);
	
	return pow;
}

static void*
domalloc(long size, ulong pc, int zero)
{
	ulong next;
	int pow, n;
	long bsize;
	Bucket *bp, *nbp;
	Info *info;

	pow = bucket(size);

	/* Allocate off this list */
	lock(&mlock);

	info = lookup(pc);

	bsize = HDRSIZE+(1<<pow);
			
	bp = arena[pow];
	if(bp) {
		arena[pow] = bp->next;
		info->n++;
		info->size += size;
		unlock(&mlock);

		assert(bp->info == 0);

		bp->info = info;
		bp->size = size;
		if(zero)
			memset(bp->data, 0, size);
		return  bp->data;
	}

	if(pow < CUTOFF) {
		n = 1<<((CUTOFF-pow)+1);
		bp = sbrk(bsize*n);
		assert((int)bp >= 0);

		next = (ulong)bp+bsize;
		nbp = (Bucket*)next;
		arena[pow] = nbp;
		for(n -= 2; n; n--) {
			next = (ulong)nbp+bsize;
			nbp->next = (Bucket*)next;
			nbp = nbp->next;
		}
	} else {
		bp = sbrk(bsize);
		assert((int)bp >= 0);
	}
	info->n++;
	info->size += size;
	unlock(&mlock);
		
	bp->size = size;
	bp->info = info;

	return bp->data;
}

static void*
dorealloc(void *ptr, long nsize, ulong pc, int zero)
{
	void *new;
	long osize;
	int opow, npow;
	Bucket *bp;

	if(ptr == 0)
		return domalloc(nsize, pc, zero);

	/* Find the start of the structure */
	bp = (Bucket*)((uint)ptr - HDRSIZE);

	assert(bp->info->magic == MAGIC);

	osize = bp->size;

	opow = bucket(osize);
	npow = bucket(nsize);

	/* enough space in this bucket */
	if(opow == npow) {
		bp->size = nsize;
		lock(&mlock);
		bp->info->size += nsize-osize;
		unlock(&mlock);
		if(zero && nsize > osize)
			memset((uchar*)ptr+osize, 0, nsize-osize);
		return ptr;
	}

	new = domalloc(nsize, pc, zero);
	if(nsize > osize)
		memmove(new, ptr, osize);
	else
		memmove(new, ptr, nsize);
		
	free(ptr);

	return new;
}

void
free(void *ptr)
{
	Bucket *bp;
	int pow;

	if(ptr == 0)
		return;

	/* Find the start of the structure */
	bp = (Bucket*)((uint)ptr - HDRSIZE);

	assert(bp->info->magic == MAGIC);

	pow = bucket(bp->size);

	lock(&mlock);

	assert(bp->info->n > 0);

	bp->info->n--;
	bp->info->size -= bp->size;
	assert(bp->info->n != 0 || bp->info->size == 0);
	bp->info = 0;


	bp->next = arena[pow];
	arena[pow] = bp;

	unlock(&mlock);
}

void*
malloc(long size)
{
	return  domalloc(size, getcallerpc(&size), 0);
}

void*
mallocz(long size)
{
	return  domalloc(size, getcallerpc(&size), 1);
}

void*
realloc(void *p, long size)
{
	return  dorealloc(p, size, getcallerpc(&size), 0);
}

void*
reallocz(void *p, long size)
{
	return  dorealloc(p, size, getcallerpc(&size), 1);
}

static Info*
lookup(ulong pc)
{
	uint h;
	Info *q;
	
	h = pc + 457*pc;
	h %= Hash;

	for(q=hash[h]; q; q=q->next) {
		if(q->pc == pc)
			return q;
	}

	q = sbrk(sizeof(Info));
	q->pc = pc;
	q->magic = MAGIC;

	q->next = hash[h];
	hash[h] = q;

	return q;
}

void
mallocstats(void)
{
	int i;
	int n, size;
	Info *p;
	Bucket *q;

	lock(&mlock);

	n = 0;
	size = 0;
	for(i = 0; i < Hash; i++) {
		for(p=hash[i]; p; p=p->next) {
			size += p->size;
			n += p->n;
		}
	}

	fprint(2, "allocated num = %d size = %d\n", n, size);
	for(i = 0; i < Hash; i++) {
		for(p=hash[i]; p; p=p->next) {
			if(p->n == 0) {
				assert(p->size == 0);
				continue;
			}
			fprint(2, "%u10x %5d %9d\n", p->pc, p->n, p->size);
		}
	}

	n = 0;
	size = 0;
	for(i=0; i<MAX2SIZE; i++) {
		for(q=arena[i]; q; q=q->next) {
			size += 1<<i;
			n++;
		}
	}	
	fprint(2, "free num = %d size = %d\n", n, size);
	for(i=0; i<MAX2SIZE; i++) {
		n = 0;
		for(q=arena[i]; q; q=q->next)
			n++;
		if(n == 0)
			continue;
		fprint(2, "     pow%.2d %5d %9d \n", i, n, n<<i);
	}	

	unlock(&mlock);
}

int
mallocleak(void)
{
	int i;
	int size;
	Info *p;

	lock(&mlock);
	size = 0;
	for(i = 0; i < Hash; i++)
		for(p=hash[i]; p; p=p->next) 
			size += p->size;
	unlock(&mlock);

	return size;
}
