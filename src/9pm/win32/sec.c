#include <u.h>
#include <9pm/libc.h>
#include <9pm/windows.h>
#include "9pm.h"

enum
{
	MAX_SID	= sizeof(SID) + SID_MAX_SUB_AUTHORITIES*sizeof(DWORD),
	BIG_SD = SECURITY_DESCRIPTOR_MIN_LENGTH
		+ sizeof(ACL) + 20*(sizeof(ACCESS_ALLOWED_ACE)+MAX_SID)
};

typedef struct	User	User;
typedef struct  Gmem	Gmem;

struct User
{
	Qlock	lk;
	SID	*sid;
	Rune	*name;
	Rune	*dom;
	int	type;
	int	gotgroup;	/* tried to add group */
	Gmem	*group;
	User	*next;
};

struct Gmem
{
	User	*user;
	Gmem	*next;
};

struct {
	NET_API_STATUS (NET_API_FUNCTION *UserGetLocalGroups)(
		LPWSTR servername,
		LPWSTR username,
		DWORD level,
		DWORD flags,
		LPBYTE *bufptr,
		DWORD prefmaxlen,
		LPDWORD entriesread,
		LPDWORD totalentries);
	NET_API_STATUS (NET_API_FUNCTION *UserGetGroups)(
		LPWSTR servername,
		LPWSTR username,
		DWORD level,
		LPBYTE *bufptr,
		DWORD prefmaxlen,
		LPDWORD entriesread,
		LPDWORD totalentries);	
	NET_API_STATUS (NET_API_FUNCTION *GetAnyDCName)(
		LPCWSTR ServerName,
		LPCWSTR DomainName,
		LPBYTE *Buffer);
	NET_API_STATUS (NET_API_FUNCTION *ApiBufferFree)(LPVOID Buffer);	
} net;

typedef struct Valmap	Valmap;
struct Valmap {
	int	val;
	char	*name;
};

SID	*dupsid(SID*);
int	ismember(Rune*, User*, SID*);
User	*sidtouser(Rune*, SID*);
User	*domnametouser(Rune*, Rune*, Rune*);
User	*nametouser(Rune*, Rune*);
void	addgroups(User*);
User	*mkuser(SID*, int, Rune*, Rune*);
Rune	*filesrv(Rune*, Rune[MAX_PATH]);
Rune	*domsrv(Rune *, Rune[MAX_PATH]);
int	fsacls(Rune*);
void perminit(int);

SID	*creatorowner;
SID	*creatorgroup;
SID	*everyone;
SID	*ntignore;
struct
{
	Qlock	lk;
	User	*u;
}users;

void
secinit(void)
{
	HMODULE lib;
	SID_IDENTIFIER_AUTHORITY id = SECURITY_CREATOR_SID_AUTHORITY;
	SID_IDENTIFIER_AUTHORITY wid = SECURITY_WORLD_SID_AUTHORITY;
	SID_IDENTIFIER_AUTHORITY ntid = SECURITY_NT_AUTHORITY;

	lib = LoadLibrary(L"netapi32");
	if(lib == 0) {
		usesecurity = 0;
		return;
	}

	net.UserGetGroups = (void*)GetProcAddress(lib, "NetUserGetGroups");
	if(net.UserGetGroups == 0)
		fatal("bad netapi32 library");
	net.UserGetLocalGroups = (void*)GetProcAddress(lib, "NetUserGetLocalGroups");
	if(net.UserGetLocalGroups == 0)
		fatal("bad netapi32 library");
	net.GetAnyDCName = (void*)GetProcAddress(lib, "NetGetAnyDCName");
	if(net.GetAnyDCName == 0)
		fatal("bad netapi32 library");
	net.ApiBufferFree = (void*)GetProcAddress(lib, "NetApiBufferFree");
	if(net.ApiBufferFree == 0)
		fatal("bad netapi32 library");
	
	AllocateAndInitializeSid(&id, 1, SECURITY_CREATOR_OWNER_RID,
		1, 2, 3, 4, 5, 6, 7, &creatorowner);
	AllocateAndInitializeSid(&id, 1, SECURITY_CREATOR_GROUP_RID,
		1, 2, 3, 4, 5, 6, 7, &creatorgroup);
	AllocateAndInitializeSid(&wid, 1, SECURITY_WORLD_RID,
		1, 2, 3, 4, 5, 6, 7, &everyone);
	AllocateAndInitializeSid(&ntid, 1, 0,
		1, 2, 3, 4, 5, 6, 7, &ntignore);

	perminit(0);
}

void
perminit(int backup)
{
	TOKEN_PRIVILEGES *priv;
	char privrock[sizeof(TOKEN_PRIVILEGES) + 1*sizeof(LUID_AND_ATTRIBUTES)];
	HANDLE token;

	/*
	 * enabling take ownership allows us to chown to ourself
	 * regargless of permissions on the file.
	 * enabling restore allows chowning to others
	 * if we have permission to write the owner.
	 */
	if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token))
		return;
	priv = (TOKEN_PRIVILEGES*)privrock;
	priv->PrivilegeCount = 1;
	priv->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	/* lets be super user */
	if(LookupPrivilegeValue(NULL, SE_TAKE_OWNERSHIP_NAME, &priv->Privileges[0].Luid))
		if(AdjustTokenPrivileges(token, 0, priv, 0, NULL, NULL))
			;
	if(backup) {
		/* netapp is broken - until it is fixed these privleges can not be enabled
		 * I do try and enable these when I do a secwperm...
		 */
		if(LookupPrivilegeValue(NULL, SE_RESTORE_NAME, &priv->Privileges[0].Luid))
			if(AdjustTokenPrivileges(token, 0, priv, 0, NULL, NULL))
				;
		if(LookupPrivilegeValue(NULL, SE_BACKUP_NAME, &priv->Privileges[0].Luid))
			if(AdjustTokenPrivileges(token, 0, priv, 0, NULL, NULL))
				;
	}
	if(LookupPrivilegeValue(NULL, SE_CHANGE_NOTIFY_NAME, &priv->Privileges[0].Luid))
		if(AdjustTokenPrivileges(token, 0, priv, 0, NULL, NULL))
			;
	CloseHandle(token);
}


int
secperm(Dir *dir, Rune *file)
{
	SECURITY_INFORMATION si;
	SECURITY_DESCRIPTOR *sd;
	ACL_SIZE_INFORMATION size;
	ACL *acl;
	ACE_HEADER *aceh;
	ACCESS_ALLOWED_ACE *ace;
	SID *sid, *osid, *gsid;
	BOOL hasacl, b;
	DWORD need, i;
	User *owner, *group;
	int allow, deny, *p, m;
	Rune *srv, srvrock[MAX_PATH];
	char sdrock[BIG_SD];

/*	secdump(file);*/

	osid = NULL;
	gsid = NULL;
	si = OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
	sd = (SECURITY_DESCRIPTOR*)sdrock;
	need = 0;
	if(!GetFileSecurity(file, si, sd, sizeof(sdrock), &need)) {
		if(GetLastError() == ERROR_ACCESS_DENIED) {
			strcpy(dir->uid, "unknown");
			strcpy(dir->gid, "unknown");
			dir->mode = 0;
			return 0;
		}
		if(GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			goto bad;
		sd = mallocz(need);
		if(!GetFileSecurity(file, si, sd, need, &need))
			goto bad;
	}
	if(!GetSecurityDescriptorOwner(sd, &osid, &b)
	|| !GetSecurityDescriptorDacl(sd, &hasacl, &acl, &b))
		goto bad;

	if(acl == 0)
		size.AceCount = 0;
	else if(!GetAclInformation(acl, &size, sizeof(size), AclSizeInformation))
		goto bad;

	srv = filesrv(file, srvrock);

	/*
	 * first pass through acl finds group
	 */
	for(i = 0; i < size.AceCount; i++){
		if(!GetAce(acl, i, &aceh))
			continue;
		if(aceh->AceFlags & INHERIT_ONLY_ACE)
			continue;

		if(aceh->AceType != ACCESS_ALLOWED_ACE_TYPE
		&& aceh->AceType != ACCESS_DENIED_ACE_TYPE)
			continue;

		ace = (ACCESS_ALLOWED_ACE*)aceh;
		sid = (SID*)&ace->SidStart;
		if(EqualSid(sid, creatorowner) || EqualSid(sid, creatorgroup))
			continue;

		if(EqualSid(sid, everyone))
			;
		else if(EqualSid(sid, osid))
			;
		else if(EqualPrefixSid(sid, ntignore))
			continue;		/* boring nt accounts */
		else{
			gsid = sid;
			break;
		}
	}
	if(gsid == NULL)
		gsid = osid;

	owner = sidtouser(srv, osid);
	group = sidtouser(srv, gsid);
	if(owner == 0 || group == 0)
		goto bad;

	/* no acl means full access */
	if(acl == 0)
		allow = 0777;
	else
		allow = 0;
	deny = 0;
	for(i = 0; i < size.AceCount; i++){
		if(!GetAce(acl, i, &aceh))
			continue;
		if(aceh->AceFlags & INHERIT_ONLY_ACE)
			continue;

		if(aceh->AceType == ACCESS_ALLOWED_ACE_TYPE)
			p = &allow;
		else if(aceh->AceType == ACCESS_DENIED_ACE_TYPE)
			p = &deny;
		else
			continue;

		ace = (ACCESS_ALLOWED_ACE*)aceh;
		sid = (SID*)&ace->SidStart;
		if(EqualSid(sid, creatorowner) || EqualSid(sid, creatorgroup))
			continue;

		m = 0;
		if(ace->Mask & FILE_EXECUTE)
			m |= 1;
		if(ace->Mask & FILE_WRITE_DATA)
			m |= 2;
		if(ace->Mask & FILE_READ_DATA)
			m |= 4;

/*
		if(ismember(srv, owner, sid))
			*p |= (m << 6) & ~(allow|deny) & 0700;
		if(ismember(srv, group, sid))
			*p |= (m << 3) & ~(allow|deny) & 0070;
*/
		if(EqualSid(osid, sid))
			*p |= (m << 6) & ~(allow|deny) & 0700;
		if(EqualSid(gsid, sid))
			*p |= (m << 3) & ~(allow|deny) & 0070;
		if(EqualSid(everyone, sid))
			*p |= m & ~(allow|deny) & 0007;
	}

	dir->mode = allow&~deny;
	wstrtoutf(dir->uid, owner->name, nelem(dir->uid));
	wstrtoutf(dir->gid, group->name, nelem(dir->gid));
	if(sd != (SECURITY_DESCRIPTOR*)sdrock)
		free(sd);
	return 0;

bad:
	winerror();
	dir->mode = 0;
	if(sd != (SECURITY_DESCRIPTOR*)sdrock)
		free(sd);
	return -1;
}

#define	NOMODE	(READ_CONTROL|FILE_READ_EA|FILE_READ_ATTRIBUTES)
#define	RMODE	(READ_CONTROL|SYNCHRONIZE\
		|FILE_READ_DATA|FILE_READ_EA|FILE_READ_ATTRIBUTES)
#define	XMODE	(READ_CONTROL|SYNCHRONIZE\
		|FILE_EXECUTE|FILE_READ_ATTRIBUTES)
#define	WMODE	(DELETE|READ_CONTROL|SYNCHRONIZE|WRITE_DAC|WRITE_OWNER\
		|FILE_WRITE_DATA|FILE_APPEND_DATA|FILE_WRITE_EA\
		|FILE_DELETE_CHILD|FILE_WRITE_ATTRIBUTES)

int
modetomask[] =
{
	NOMODE,
	XMODE,
	WMODE,
	WMODE|XMODE,
	RMODE,
	RMODE|XMODE,
	RMODE|WMODE,
	RMODE|WMODE|XMODE,
};

int
secwperm(Rune *file, Dir *dir, int isdir)
{
	int m;
	BOOL b;
	ACL *dacl;
	SID *osid;
	ulong mode;
	DWORD need;
	User *ou, *gu;
	ACE_HEADER *aceh;
	SECURITY_DESCRIPTOR *sd;
	SECURITY_INFORMATION si;
	Rune *srv, srvrock[MAX_PATH], name[MAX_PATH];
	char sdrock[SECURITY_DESCRIPTOR_MIN_LENGTH + MAX_SID];
	char aclrock[sizeof(ACL)+4*(sizeof(ACCESS_ALLOWED_ACE)+2*MAX_SID)];

	perminit(1);

	dacl = 0;

	if(!fsacls(file))
		return 0;

	srv = filesrv(file, srvrock);
	utftowstr(name, dir->uid, nelem(name));
	ou = nametouser(srv, name);
	if(ou == 0)
		return -1;
	utftowstr(name, dir->gid, nelem(name));
	gu = nametouser(srv, name);
	if(gu == 0) {
		if (strcmp(dir->gid, "unknown") != 0
		&&  strcmp(dir->gid, "deleted") != 0)
			return -1;
		gu = ou;
	}


	sd = (SECURITY_DESCRIPTOR*)sdrock;
	need = 0;
	osid = 0;
	if(GetFileSecurity(file, OWNER_SECURITY_INFORMATION, sd, sizeof(sdrock), &need))
		GetSecurityDescriptorOwner(sd, &osid, &b);

	dacl = (ACL*)aclrock;
	if(!InitializeAcl(dacl, sizeof(aclrock), ACL_REVISION2)){
		winerror();
		return -1;
	}

	mode = dir->mode;
	if(ou == gu){
		mode |= (mode >> 3) & 0070;
		mode |= (mode << 3) & 0700;
	}

	m = modetomask[(mode>>6) & 7];
	if(!AddAccessAllowedAce(dacl, ACL_REVISION2, m, ou->sid)){
		winerror();
		return -1;
	}

	if(isdir && !AddAccessAllowedAce(dacl, ACL_REVISION2, m, creatorowner)){
		winerror();
		return -1;
	}

	m = modetomask[(mode>>3) & 7];
	if(!AddAccessAllowedAce(dacl, ACL_REVISION2, m, gu->sid)){
		winerror();
		return -1;
	}

	m = modetomask[(mode>>0) & 7];
	if(!AddAccessAllowedAce(dacl, ACL_REVISION2, m, everyone)){
		winerror();
		return -1;
	}

	if(isdir) {
		/* hack to add inherit flags */
		if(GetAce(dacl, 1, &aceh))
			aceh->AceFlags |= OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE;
		if(GetAce(dacl, 2, &aceh))
			aceh->AceFlags |= OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE;
		if(GetAce(dacl, 3, &aceh))
			aceh->AceFlags |= OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE;
	}

	if(!InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION)){
		winerror();
		return -1;
	}
	if(!SetSecurityDescriptorDacl(sd, 1, dacl, 0)){
		winerror();
		return -1;
	}

	si = DACL_SECURITY_INFORMATION;
	if(osid == NULL || !EqualSid(osid, ou->sid)){
		si |= OWNER_SECURITY_INFORMATION;
		if(!SetSecurityDescriptorOwner(sd, ou->sid, 0)){
			winerror();
			return -1;
		}
	}
	if(!SetFileSecurity(file, si, sd)){
		/* two steps will sometimes work where one will not */
		if((si & OWNER_SECURITY_INFORMATION)
		&& !SetFileSecurity(file, OWNER_SECURITY_INFORMATION, sd)) {
			winerror();
			return -1;
		}
		if(!SetFileSecurity(file, DACL_SECURITY_INFORMATION, sd)) {
			winerror();
			return -1;
		}

	}

	return 0;
}

User*
sidtouser(Rune *srv, SID *s)
{
	SID_NAME_USE type;
	WCHAR aname[100], dname[100];
	DWORD naname, ndname;
	User *u;

	qlock(&users.lk);
	for(u = users.u; u != 0; u = u->next)
		if(EqualSid(s, u->sid))
			break;
	qunlock(&users.lk);

	if(u != 0)
		return u;

	naname = sizeof(aname);
	ndname = sizeof(dname);

	if(!LookupAccountSid(srv, s, aname, &naname, dname, &ndname, &type)) {
		winerror();
		return 0;
	}
	return mkuser(s, type, aname, dname);
}

User*
domnametouser(Rune *srv, Rune *name, Rune *dom)
{
	User *u;

	qlock(&users.lk);
	for(u = users.u; u != 0; u = u->next)
		if(wstrcmp(name, u->name) == 0 && wstrcmp(dom, u->dom) == 0)
			break;
	qunlock(&users.lk);
	if(u == 0)
		u = nametouser(srv, name);
	return u;
}

User*
nametouser(Rune *srv, Rune *name)
{
	char sidrock[MAX_SID];
	SID *sid;
	SID_NAME_USE type;
	Rune dom[MAX_PATH];
	DWORD nsid, ndom;

	sid = (SID*)sidrock;
	nsid = sizeof(sidrock);
	ndom = sizeof(dom);
	if(!LookupAccountName(srv, name, sid, &nsid, dom, &ndom, &type)){
		winerror();
		return NULL;
	}

	return mkuser(sid, type, name, dom);
}

User*
mkuser(SID *sid, int type, Rune *name, Rune *dom)
{
	User *u;

	qlock(&users.lk);
	for(u = users.u; u != 0; u = u->next){
		if(EqualSid(sid, u->sid)){
			qunlock(&users.lk);
			return u;
		}
	}

	switch(type) {
	default:
		break;
	case SidTypeDeletedAccount:
		name = L"deleted";
		break;
	case SidTypeInvalid:
		name = L"invalid";
		break;
	case SidTypeUnknown:
		name = L"unknown";
		break;
	}

	u = mallocz(sizeof(User));
	if(u == 0){
		qunlock(&users.lk);
		return 0;
	}
	u->next = 0;
	u->group = 0;
	u->sid = dupsid(sid);
	u->type = type;
	u->name = 0;
	if(name != 0)
		u->name = wstrdup(name);
	u->dom = 0;
	if(dom != 0)
		u->dom = wstrdup(dom);

	u->next = users.u;
	users.u = u;

	qunlock(&users.lk);
	return u;
}

int
ismember(Rune *srv, User *u, SID *gsid)
{
	User *g;
	Gmem *grps;

	if(EqualSid(u->sid, gsid))
		return 1;
	if(EqualSid(gsid, everyone))
		return 1;

	g = sidtouser(srv, gsid);
	if(g == 0)
		return 0;

	qlock(&u->lk);
	addgroups(u);
	for(grps = u->group; grps != 0; grps = grps->next){
		if(EqualSid(grps->user->sid, gsid)){
			qunlock(&u->lk);
			return 1;
		}
	}
	qunlock(&u->lk);
	return 0;
}

void
addgroups(User *u)
{
	LOCALGROUP_USERS_INFO_0 *loc;
	GROUP_USERS_INFO_0 *grp;
	DWORD i, n, rem;
	User *gu;
	Gmem *g;
	Rune *srv, srvrock[MAX_PATH];

	assert(holdqlock(&u->lk));
	if(u->gotgroup)
		return;
	u->gotgroup = 1;

	rem = 1;
	n = 0;
	srv = domsrv(u->dom, srvrock);
	while(rem != n){
		i = net.UserGetGroups(srv, u->name, 0,
			(BYTE**)&grp, 1024, &n, &rem);
		if(i != NERR_Success && i != ERROR_MORE_DATA)
			break;
		for(i = 0; i < n; i++){
			gu = domnametouser(srv, grp[i].grui0_name, u->dom);
			if(gu == 0)
				continue;
			g = mallocz(sizeof(Gmem));
			g->user = gu;
			g->next = u->group;
			u->group = g;
		}
		net.ApiBufferFree(grp);
	}
	rem = 1;
	n = 0;
	while(rem != n){
		i = net.UserGetLocalGroups(srv, u->name, 0, LG_INCLUDE_INDIRECT,
			(BYTE**)&loc, 1024, &n, &rem);
		if(i != NERR_Success && i != ERROR_MORE_DATA)
			break;
		for(i = 0; i < n; i++){
			gu = domnametouser(srv, loc[i].lgrui0_name, u->dom);
			if(gu == NULL)
				continue;
			g = mallocz(sizeof(Gmem));
			g->user = gu;
			g->next = u->group;
			u->group = g;
		}
		net.ApiBufferFree(loc);
	}
}

SID*
dupsid(SID *sid)
{
	SID *nsid;
	int n;

	n = GetLengthSid(sid);
	nsid = mallocz(n);
	if(!CopySid(n, nsid, sid))
		winerror();
	return nsid;
}

Rune*
filesrv(Rune *file, Rune srv[MAX_PATH])
{
	Rune *p;
	int n;
	Rune vol[3], uni[MAX_PATH];

	/* assume file is a fully qualified name - X: or \\server */
	if(file[1] == ':') {
		vol[0] = file[0];
		vol[1] = file[1];
		vol[2] = 0;
		if(GetDriveType(vol) != DRIVE_REMOTE)
			return 0;
		n = sizeof(uni);
		if(WNetGetUniversalName(vol, UNIVERSAL_NAME_INFO_LEVEL, uni, &n) != NO_ERROR)
			return 0;
		file = ((UNIVERSAL_NAME_INFO*)uni)->lpUniversalName;
	}
	p = wstrchr(file+2, '\\');
	if(p == 0)
		n = wstrlen(file+2);
	else
		n = p-(file+2);
	if(n >= MAX_PATH)
		n = MAX_PATH-1;
	memcpy(srv, file+2, n*sizeof(Rune));
	srv[n] = 0;
	return srv;
}

int
fsacls(Rune *file)
{
	Rune *p;
	DWORD flags;
	Rune path[MAX_PATH];

	/* assume file is a fully qualified name - X: or \\server */
	if(file[1] == ':') {
		path[0] = file[0];
		path[1] = file[1];
		path[2] = '\\';
		path[3] = 0;
	} else {
		wstrcpy(path, file);
		p = wstrchr(path+2, '\\');
		if(p == 0)
			return 0;
		p = wstrchr(p+1, '\\');
		if(p == 0)
			wstrcat(path, L"\\");
		else
			p[1] = 0;
	}
	if(!GetVolumeInformation(path, NULL, 0, NULL, NULL, &flags, NULL, 0))
		return 0;

	return flags & FS_PERSISTENT_ACLS;
}

Rune*
domsrv(Rune *dom, Rune srv[MAX_PATH])
{
	Rune *psrv;
	int n, r;

	if(dom[0] == 0)
		return 0;

	r = net.GetAnyDCName(NULL, dom, (LPBYTE*)&psrv);
	if(r == NERR_Success) {
		n = wstrlen(psrv);
		if(n >= MAX_PATH)
			n = MAX_PATH-1;
		memcpy(srv, psrv, n*sizeof(Rune));
		srv[n] = 0;
		net.ApiBufferFree(psrv);
		return srv;
	}

	return 0;
}
