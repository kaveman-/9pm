#include "win.h"

#undef FormatMessage
DWORD WINAPI
FormatMessage(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId,
	DWORD dwLanguageId, LPWSTR lpBuffer, DWORD nSize, va_list *Arguments)
{
	char buf[MAX_PATH];
	int n;

	assert(Arguments == 0);

	if(win_useunicode)
		return FormatMessageW(dwFlags, lpSource, dwMessageId,
			dwLanguageId, lpBuffer, nSize, Arguments);

	buf[0] = 0;

	n = FormatMessageA(dwFlags, lpSource, dwMessageId,
			dwLanguageId, buf, sizeof(buf), Arguments);
	if(n == 0) {
		lpBuffer[0] = 0;
		return 0;
	}

	n = win_utf2wstr(lpBuffer, nSize, buf);
	if(n >= (int)nSize)
		return 0;
	return n;
}
