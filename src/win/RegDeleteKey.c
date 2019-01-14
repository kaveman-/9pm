#include "win.h"

#undef RegDeleteKey

LONG APIENTRY
RegDeleteKey(HKEY hKey, LPCWSTR lpSubKey)
{
	char subkey[MAX_PATH];

	if(win_useunicode)
		return RegDeleteKeyW(hKey, lpSubKey);

	win_wstr2utf(subkey, MAX_PATH, (Rune*)lpSubKey);
	return RegDeleteKeyA(hKey, subkey);
}
