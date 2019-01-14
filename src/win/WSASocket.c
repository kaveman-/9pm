#include "win.h"

#undef WSASocket

SOCKET PASCAL
WSASocket(int af, int type, int protocol, LPWSAPROTOCOL_INFO lpProtocolInfo, GROUP g, DWORD dwFlags)
{
	if(win_useunicode)
		return WSASocketW(af, type, protocol, lpProtocolInfo, g, dwFlags);

	win_fatal("WSASocketA: not done yet");
}
