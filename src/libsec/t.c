#include <stdio.h>
#include <windows.h>

typedef unsigned __int64 uvlong;

__declspec( naked ) void
rdtsc2(uvlong *p)
{
	_asm {
		mov ecx, [esp+4]

		_emit 0x0F      ;RDTSC - get beginning timestamp to edx:eax
		_emit 0x31

		mov [ecx+4],edx  ;save beginning timestamp (1 cycle)
		mov [ecx],eax
		ret
        }
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

uvlong
foo(uvlong x)
{
	return x + 1;
}

void
main(void)
{
	uvlong a[10000][3];
	int i;
	LARGE_INTEGER ti;

	for (i=0; i<100; i++) {
		a[i][0] = rdtsc();
		QueryPerformanceCounter(&ti);
		a[i][1] = ti.QuadPart;
		a[i][2] = GetTickCount();
		Sleep(10);
	}
	for(i=1; i<100; i++)
		printf("%I64d %I64d %I64d %I64d %I64d\n", a[i][0], a[i][1], a[i][2], a[i][0] - a[i-1][0], a[i][1] - a[i-1][1]);
}
