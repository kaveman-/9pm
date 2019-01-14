#include "os.h"
#include <windows.h>
#include <mp.h>
#include <libsec.h>

typedef struct State{
	uvlong		seed;
	DES3state	des3;
} State;

static int init;
static CRITICAL_SECTION cs;
static double fasttick;

static State x917state;

static uvlong
nsec(void)
{
	LARGE_INTEGER ti;
	FILETIME ft;
	uvlong t;
	
	GetSystemTimeAsFileTime(&ft);
	t = (((uvlong)ft.dwLowDateTime) + (((uvlong)ft.dwHighDateTime)<<32))*100;

	if(fasttick != 0 && QueryPerformanceCounter(&ti)) {
		t += (uvlong)(((double)ti.QuadPart)*fasttick);
		t -= ((uvlong)GetTickCount())*1000000;
	}

	return t;
}

__declspec( naked ) uvlong
rdtsc(void)
{
	_asm {
		_emit 0x0F      ;RDTSC - get beginning timestamp to edx:eax
		_emit 0x31
		ret
        }
}

/*
 * attempt at generating a pretty good random seed
 * the following assumes we are on a pentium - should do
 * something if we do not have the rdtsc instruction
 */
static void
randomseed(uchar *p)
{
	SHAstate *s;
	uvlong tsc;
	LARGE_INTEGER ti;
	int i, j;
	FILETIME ft;

	GetSystemTimeAsFileTime(&ft);
	s = sha1((uchar*)&ft, sizeof(ft), 0, 0);
	for(i=0; i<50; i++) {
		for(j=0; j<10; j++) {
			tsc = rdtsc();
			s = sha1((uchar*)&tsc, sizeof(tsc), 0, s);
			QueryPerformanceCounter(&ti);
			s = sha1((uchar*)&ti, sizeof(ti), 0, s);
			tsc = GetTickCount();
			s = sha1((uchar*)&tsc, sizeof(tsc), 0, s);
		}
		Sleep(10);
	}
	sha1(0, 0, p, s);
}


static void
X917(uchar *rand, int nrand)
{
	int i, m, n8;
	uvlong I, x;

	/* 1. Compute intermediate value I = Ek(time). */
	I = nsec();
	triple_block_cipher(x917state.des3.expanded, (uchar*)&I, 0); /* two-key EDE */

	/* 2. x[i] = Ek(I^seed);  seed = Ek(x[i]^I); */
	m = (nrand+7)/8;
	for(i=0; i<m; i++){
		x = I ^ x917state.seed;
		triple_block_cipher(x917state.des3.expanded, (uchar*)&x, 0);
		n8 = (nrand>8) ? 8 : nrand;
		memcpy(rand, (uchar*)&x, n8);
		rand += 8;
		nrand -= 8;
		x ^= I;
		triple_block_cipher(x917state.des3.expanded, (uchar*)&x, 0);
		x917state.seed = x;
	}
}

static void
X917init(void)
{
	uchar mix[128];
	uchar key3[3][8];
	uchar seed[SHA1dlen];

	randomseed(seed);
	memmove(key3, seed, SHA1dlen);
	setupDES3state(&x917state.des3, key3, nil);
	X917(mix, sizeof mix);
}

static void
genrandominit(void)
{
	static long lock;
	LARGE_INTEGER fi;
	

	/*
	 * initialize only once
	 * have to roll my own lock!
	 * The CRITICAL_SECTION object appears to require
	 * a single thread to initial it -
	 * sort of hard to do without a synchronization
	 * primitive!!
	 */
		
	while(InterlockedExchange(&lock, 1) != 0)
		Sleep(100);

	if(!init) {
		InitializeCriticalSection(&cs);

		if(QueryPerformanceFrequency(&fi))
		if(fi.QuadPart != 0)
			fasttick = 1.e9/fi.QuadPart;

		X917init();
		
		init = 1;
	}

	InterlockedExchange(&lock, 0);
}

void
genrandom(uchar *p, int n)
{
	if(!init)
		genrandominit();
	EnterCriticalSection(&cs);
	X917(p, n);
	LeaveCriticalSection(&cs);
}
