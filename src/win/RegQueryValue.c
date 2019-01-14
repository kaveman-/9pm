#include "win.h"

#undef RegQueryValue

LONG APIENTRY
RegQueryValue(HKEY hKey, LPCWSTR lpSubKey, LPWSTR lpValue, PLONG lpcbValue)
{
	char *key, keyrock[MAX_PATH];
	char *value, valuerock[MAX_PATH];
	long n, i, res;

	if(win_useunicode)
		return RegQueryValueW(hKey, lpSubKey, lpValue, lpcbValue);

	if(lpSubKey) {
		key = keyrock;
		win_wstr2utf(key, MAX_PATH, (Rune*)lpSubKey);
	} else
		key = 0;
	n = *lpcbValue;	
	n >>= 1;
	if(n <= sizeof(valuerock))
		value = valuerock;
	else
		value = malloc(n);

	value[0] = 0;
	res = RegQueryValueA(hKey, key, value, &n);
	if(res == 0) {
		for(i=0; value[i]; i++)
			lpValue[i] = value[i];
		lpValue[i] = 0;
		*lpcbValue = i*sizeof(Rune);
	}

	if(value != valuerock)
		free(value);
	return res;
}
