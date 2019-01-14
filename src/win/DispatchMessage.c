#include "win.h"


#undef DispatchMessage
LONG WINAPI 
DispatchMessage(CONST MSG *lpMsg)
{
	if(win_useunicode)
		return DispatchMessageW(lpMsg);

	return DispatchMessageA(lpMsg);
}
