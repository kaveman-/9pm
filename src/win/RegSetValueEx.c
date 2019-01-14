#include "win.h"

#undef RegSetValueEx

LONG APIENTRY
RegSetValueEx(HKEY hKey, LPCWSTR lpValueName, DWORD Reserved, DWORD dwType, BYTE* lpData, DWORD cbData)
{
	char *name, namebuf[MAX_PATH];
	void *data;
	int n;
	LONG res;

	if(win_useunicode)
		return RegSetValueExW(hKey, lpValueName, Reserved, dwType, lpData, cbData);
	
	if(lpValueName) {
		win_wstr2utf(namebuf, MAX_PATH, (Rune*)lpValueName);
		name = namebuf;
	} else
		name = 0;

	switch(dwType) {
	default:
		win_fatal("RegSetValueEx: unknown data type");
	case REG_BINARY:
	case REG_DWORD:
//	case REG_DWORD_LITTLE_ENDIAN:
	case REG_DWORD_BIG_ENDIAN:
	case REG_LINK:
	case REG_NONE:
		data = lpData;
		n = cbData;
		break;
	case REG_EXPAND_SZ:
	case REG_SZ:
	case REG_MULTI_SZ:
	case REG_RESOURCE_LIST:
		win_fatal("RegSetValueEx: not done yet");
	}

	res = RegSetValueExA(hKey, name, Reserved, dwType, data, n);

	if(data != lpData)
		; // free data
	return res;
}


