#include "win.h"

#undef RegOpenKey

LONG APIENTRY
RegOpenKey(HKEY hKey, LPCWSTR lpSubKey, PHKEY phkResult)
{
	char subkey[MAX_PATH];

	if(win_useunicode)
		return RegOpenKeyW(hKey, lpSubKey, phkResult);

	win_wstr2utf(subkey, MAX_PATH, (Rune*)lpSubKey);
	return RegOpenKeyA(hKey, subkey, phkResult);
}
