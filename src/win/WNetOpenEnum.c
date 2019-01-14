#include "win.h"

#undef WNetOpenEnum
DWORD WINAPI
WNetOpenEnum(DWORD dwScope, DWORD dwType, DWORD dwUsage,
	LPNETRESOURCEW lpNetResource, LPHANDLE lphEnum)
{
	NETRESOURCEA res, *p;
	char lname[MAX_PATH], rname[MAX_PATH], comment[MAX_PATH], provider[MAX_PATH];

	if(win_useunicode)
		return WNetOpenEnumW(dwScope, dwType, dwUsage,
			lpNetResource, lphEnum);

	if(lpNetResource != 0) {
		p = &res;
		memset(&res, 0, sizeof(res));
		res.dwScope = lpNetResource->dwScope;
		res.dwType = lpNetResource->dwType;
		res.dwDisplayType = lpNetResource->dwDisplayType;
		res.dwUsage = lpNetResource->dwUsage;
		if(lpNetResource->lpLocalName) {
			win_wstr2utf(lname, sizeof(lname), lpNetResource->lpLocalName);
			res.lpLocalName = lname;
		}
		if(lpNetResource->lpRemoteName) {
			win_wstr2utf(rname, sizeof(rname), lpNetResource->lpRemoteName);
			res.lpRemoteName = rname;
		}
		if(lpNetResource->lpComment) {
			win_wstr2utf(comment, sizeof(comment), lpNetResource->lpComment);
			res.lpComment = comment;
		}
		if(lpNetResource->lpProvider) {
			win_wstr2utf(provider, sizeof(provider), lpNetResource->lpProvider);
			res.lpProvider = provider;
		}
	} else
		p = 0;

	return WNetOpenEnumA(dwScope, dwType, dwUsage, p, lphEnum);
}
