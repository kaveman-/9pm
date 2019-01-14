#include "win.h"

#undef ChooseFont

BOOL WINAPI
ChooseFont(LPCHOOSEFONT lpcf)
{
	if(win_useunicode)
		return ChooseFontW(lpcf);

	win_fatal("ChooseFontA: not done yet");
}
