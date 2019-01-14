#include "win.h"

#undef RegCreateKey

LONG APIENTRY
RegCreateKey(HKEY hKey, LPCWSTR lpSubKey, PHKEY phkResult)
{
	char subkey[MAX_PATH];

	if(win_useunicode)
		return RegCreateKeyW(hKey, lpSubKey, phkResult);

	win_wstr2utf(subkey, MAX_PATH, (Rune*)lpSubKey);
	return RegCreateKeyA(hKey, subkey, phkResult);
}
