#include <u.h>
#include <9pm/windows.h>

extern BOOL WINAPI DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);


BOOL WINAPI
_DllMainCRTStartup(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	return DllMain(hModule, ul_reason_for_call, lpReserved);
}
