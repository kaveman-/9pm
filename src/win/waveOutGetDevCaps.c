#include "win.h"

#undef waveOutGetDevCaps
MMRESULT WINAPI
waveOutGetDevCaps(UINT uDeviceID, LPWAVEOUTCAPSW pwoc, UINT cbwoc)
{
	WAVEOUTCAPSA cap;
	int r, i;

	if(win_useunicode)
		return waveOutGetDevCapsW(uDeviceID, pwoc, cbwoc);
	
	r = waveOutGetDevCapsA(uDeviceID, &cap, cbwoc);

	if(r != MMSYSERR_NOERROR)
		return r;
	
	pwoc->wMid = cap.wMid;
	pwoc->wPid = cap.wPid;
	pwoc->vDriverVersion = cap.vDriverVersion;
	for(i=0; i<MAXPNAMELEN; i++)
		pwoc->szPname[i] = cap.szPname[i];
	pwoc->dwFormats = cap.dwFormats;
	pwoc->wChannels = cap.wChannels;
	pwoc->wReserved1 = cap.wReserved1;
	pwoc->dwSupport = cap.dwSupport;

	return r;
}
