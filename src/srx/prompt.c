#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <mp.h>
#include <libsec.h>

#include "srx.h"
#include "resource.h"

#pragma comment( lib, "user32.lib" )
#pragma comment( lib, "advapi32.lib")

enum {
	WM_SETSTATUS = WM_USER+1000,
	WM_AUTHENT = WM_USER+1001,
};

typedef struct Info Info;

struct Info {
	Connection *c;
	int id;
	char user[100];
	char password[100];
};

static void checkThread(long);

static HWND hDialog;
static Info info;

BOOL CALLBACK
passwordDlgProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	Info *ip;

	switch(iMsg) {
	case WM_INITDIALOG:
		info.id = 0;
		hDialog = hDlg;
		SetDlgItemText(hDlg, IDC_USERNAME, info.user);
		SetDlgItemText(hDlg, IDC_STATUS, " Not authenticated");
		ShowWindow(hDlg, SW_SHOW);
		return 1;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			EnableWindow(GetDlgItem(hDlg, IDOK), 0);
			EnableWindow(GetDlgItem(hDlg, IDSTOP), 1);
			SetDlgItemText(hDlg, IDC_STATUS, " Start authentication");
			if(info.id == 0)
				EnableWindow(GetDlgItem(hDlg, IDC_USERNAME), 0);
			GetDlgItemText(hDlg, IDC_USERNAME, info.user, sizeof(info.user));
			GetDlgItemText(hDlg, IDC_PASSWORD, info.password, sizeof(info.password));
			ip = blMallocZ(sizeof(Info));
			*ip = info;
			blThreadCreate(checkThread, (long)ip);
			break;
		case IDSTOP:
			info.id++;
			EnableWindow(GetDlgItem(hDlg, IDOK), 1);
			EnableWindow(GetDlgItem(hDlg, IDSTOP), 0);
			SetDlgItemText(hDlg, IDC_STATUS, " Request Halted");
			break;
		case IDCANCEL:
			blSetError("auth", "user name and password not checked");
			EndDialog(hDlg, 0);
			break;
		}
		return 0;
	case WM_SETSTATUS:
		if((int)wParam != info.id) {
			SetWindowLong(hDlg, DWL_MSGRESULT, 0);
			return 1;
		}
		SetDlgItemText(hDlg, IDC_STATUS, (char*)lParam);
		SetWindowLong(hDlg, DWL_MSGRESULT, 1);
 		return 1;
	case WM_AUTHENT:
		ip = (Info*)(lParam);
		if(ip->id != info.id)
			return 1;
		if(wParam) {
			info = *ip;
			EndDialog(hDlg, 1);
		} else {
			info.id++;
			EnableWindow(GetDlgItem(hDlg, IDOK), 1);
			EnableWindow(GetDlgItem(hDlg, IDSTOP), 0);
			blSleep(200); // a little time to notice
		}
		return 1;
	}
	return 0;
}


int
auth(Connection *c)
{
	int r, n;

	info.c = c;
	n = sizeof(info.user);
	if(!GetUserName(info.user, &n)) {
		blOSError("GetUserName");
		return 0;
	}

	r = DialogBox(GetModuleHandle(0), MAKEINTRESOURCE(IDD_PASSWORD), GetDesktopWindow(), passwordDlgProc);
	if(r < 0) {
		blOSError("DialogBox");
		return 0;
	}

	// user selected cancel
	if(r == 0)
		exit(1);

	return 1;
}

void
put_ssh_cmsg_user(Connection *c, char *user)
{
	Packet *packet;

	packet = (Packet *)blMallocZ(1024);
	packet->type = SSH_CMSG_USER;
	packet->length = 0;
	packet->pos = packet->data;

	debug("sending User message, user is %s\n", user);

	putstring(packet, user, strlen(user));
	putpacket(c, packet);
}

void
put_ssh_cmsg_auth_password(Connection *c, char *password)
{
	Packet *packet;

	packet = (Packet *)blMallocZ(1024);
	packet->type = SSH_CMSG_AUTH_PASSWORD;
	packet->length = 0;
	packet->pos = packet->data;

	putstring(packet, password, strlen(password));
	putpacket(c, packet);
}

static int
check(Info *ip)
{
	if(ip->id == 0) {
		put_ssh_cmsg_user(ip->c, ip->user);
		if(get_ssh_smsg_success(ip->c))
			return 1;
	}
	put_ssh_cmsg_auth_password(ip->c, ip->password);

	if(!get_ssh_smsg_success(ip->c)) {
		SendMessage(hDialog, WM_SETSTATUS, ip->id, (long)" Incorrect user name or password");
		return 0;
	}
	return 1;
}

static void
checkThread(long a)
{
	Info *ip = (Info*)a;
	int r;

	r = check(ip);
	SendMessage(hDialog, WM_AUTHENT, r, (long)ip);
	blFree(ip);
	blThreadExit();
}

