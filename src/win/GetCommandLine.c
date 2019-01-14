#include "win.h"

#undef GetCommandLine
LPWSTR WINAPI
GetCommandLine(VOID)
{
	/* GetCommandLineW is implemented on Windows 95 */
	return GetCommandLineW();
}

