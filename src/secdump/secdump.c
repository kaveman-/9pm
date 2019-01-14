#define UNICODE
#include <windows.h>
#include <lm.h>
#include <stdio.h>

enum{
	MAX_SID		= sizeof(SID) + SID_MAX_SUB_AUTHORITIES*sizeof(DWORD),
};

#define	ARGBEGIN	for(argv++,argc--;\
			    argv[0] && argv[0][0]=='-' && argv[0][1];\
			    argc--, argv++) {\
				char *_args, *_argt;\
				int _argc;\
				_args = &argv[0][1];\
				if(_args[0]=='-' && _args[1]==0){\
					argc--; argv++; break;\
				}\
				_argc = 0;\
				while(_argc = *_args++)\
				switch(_argc)
#define	ARGEND		}
#define	ARGF()		(_argt=_args, _args="",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): 0))
#define	ARGC()		_argc

#define	nelem(x)	(sizeof(x)/sizeof((x)[0]))

typedef	unsigned short	Rune;
typedef	unsigned long	ulong;

#define	nil		((void*)0)
#define UTFmax		3

typedef	struct	Valmap	Valmap;
struct Valmap
{
	int	val;
	char	*name;
};

typedef struct	User	User;
typedef	struct	Gmem	Gmem;
struct User
{
	SID	*sid;
	Rune	*name;
	Rune	*dom;
	int	type;
	int	gotgroup;	/* tried to add group */
	Gmem	*group;		/* global and local groups to which this user or group belongs. */
	User	*next;
};

struct Gmem
{
	User	*user;
	Gmem	*next;
};

void	dumpace(Rune*, void*);
void	usage(void);
void	secdump(Rune*);
void	fatal(char*, ...);
void	error(char*, ...);
char	*winerror(void);
void	dumpsid(Rune*, char*, SID*);
void	dumpacl(Rune*, char*, ACL*);
void	printvalmap(Valmap*, int, int);
void	printvalflags(Valmap*, int, int);
void	printaccess(ulong);
void	seccheck(Rune*, Rune*);
Rune*	pathcanonic(Rune *path, Rune *pathrock);

static	Rune		*filesrv(Rune*);
static	SID		*dupsid(SID*);
static	int		ismembersid(Rune*, User*, SID*);
static	int		ismember(User*, User*);
static	User		*sidtouser(Rune*, SID*);
static	User		*domnametouser(Rune*, Rune*, Rune*);
static	User		*nametouser(Rune*, Rune*);
static	void		addgroups(User*, int);
static	User		*mkuser(SID*, int, Rune*, Rune*);
static	Rune		*domsrv(Rune *, Rune[MAX_PATH]);

	int		runeslen(Rune*);
	Rune*		runesdup(Rune*);
	int		runescmp(Rune*, Rune*);
	Rune*		runeschr(Rune*, int);
	Rune*		utftorunes(char*);

SID	*creatorowner;
SID	*creatorgroup;
SID	*everyone;
SID	*ntignore;
SID	*ntbuiltin;
User	*users;

/*
 * these lan manager functions are not supplied
 * on windows95, so we have to load the dll by hand
 */
static struct {
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

void
usage(void)
{
	fprintf(stderr, "usage: sec [-u user] file ...\n");
	exit(1);
}

void
init(void)
{
	HMODULE lib;
	SID_IDENTIFIER_AUTHORITY id = SECURITY_CREATOR_SID_AUTHORITY;
	SID_IDENTIFIER_AUTHORITY wid = SECURITY_WORLD_SID_AUTHORITY;
	SID_IDENTIFIER_AUTHORITY ntid = SECURITY_NT_AUTHORITY;

	AllocateAndInitializeSid(&id, 1, SECURITY_CREATOR_OWNER_RID,
		1, 2, 3, 4, 5, 6, 7, &creatorowner);
	AllocateAndInitializeSid(&id, 1, SECURITY_CREATOR_GROUP_RID,
		1, 2, 3, 4, 5, 6, 7, &creatorgroup);
	AllocateAndInitializeSid(&wid, 1, SECURITY_WORLD_RID,
		1, 2, 3, 4, 5, 6, 7, &everyone);
	AllocateAndInitializeSid(&ntid, 1, 0,
		1, 2, 3, 4, 5, 6, 7, &ntignore);
	AllocateAndInitializeSid(&ntid, 2, SECURITY_BUILTIN_DOMAIN_RID,
		1, 2, 3, 4, 5, 6, 7, &ntbuiltin);

	lib = LoadLibrary(L"netapi32");
	if(lib == 0)
		fatal("no netapi32 library");

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
}

void
main(int argc, char *argv[])
{
	Rune *user, *path, pathrock[MAX_PATH];
	int i, dump;

	user = nil;
	dump = 0;
	ARGBEGIN{
	case 'u':
		user = utftorunes(ARGF());
		break;
	default:
		usage();
	}ARGEND

	if(argc < 1)
		usage();
	init();
	for(i = 0; i < argc; i++){
		path = pathcanonic(utftorunes(argv[i]), pathrock);
		if(path == nil)
			continue;
		if(user)
			seccheck(user, path);
		else
			secdump(path);
	}
}

void
seccheck(Rune *user, Rune *file)
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
	User *u;
	Rune *srv;
	int allow, deny, *p, m;

	osid = NULL;
	gsid = NULL;
	si = OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
	GetFileSecurity(file, si, NULL, 0, &need);
	sd = malloc(need);
	if(!GetFileSecurity(file, si, sd, need, &need)
	|| !GetSecurityDescriptorDacl(sd, &hasacl, &acl, &b)
	|| !GetAclInformation(acl, &size, sizeof(size), AclSizeInformation))
		return;

	srv = filesrv(file);
	allow = 0;
	deny = 0;
	u = nametouser(srv, user);
	if(u == nil)
		return;
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

		m = ace->Mask;

		if(EqualSid(everyone, sid) || ismembersid(srv, u, sid))
			*p |= m & ~(allow|deny);
	}

	allow &= ~deny;
	printaccess(allow);
	free(sd);
}

/*
 * the user manipulation routines
 * just make it easier to deal with user identities
 */
static User*
sidtouser(Rune *srv, SID *s)
{
	SID_NAME_USE type;
	Rune aname[100], dname[100];
	DWORD naname, ndname;
	User *u;

	for(u = users; u != 0; u = u->next)
		if(EqualSid(s, u->sid))
			break;

	if(u != 0)
		return u;

	naname = sizeof(aname);
	ndname = sizeof(dname);

	if(!LookupAccountSid(srv, s, aname, &naname, dname, &ndname, &type))
		return nil;
	return mkuser(s, type, aname, dname);
}

static User*
domnametouser(Rune *srv, Rune *name, Rune *dom)
{
	User *u;

	for(u = users; u != 0; u = u->next)
		if(runescmp(name, u->name) == 0 && runescmp(dom, u->dom) == 0)
			break;
	if(u == nil)
		u = nametouser(srv, name);
	return u;
}

static User*
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
	if(!LookupAccountName(srv, name, sid, &nsid, dom, &ndom, &type))
		return nil;

	return mkuser(sid, type, name, dom);
}

/*
 * make a user structure and add it to the global cache.
 */
static User*
mkuser(SID *sid, int type, Rune *name, Rune *dom)
{
	User *u;

	for(u = users; u != 0; u = u->next){
		if(EqualSid(sid, u->sid)){
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

	u = malloc(sizeof(User));
	if(u == nil){
		return 0;
	}
	u->next = nil;
	u->group = nil;
	u->sid = dupsid(sid);
	u->type = type;
	u->name = nil;
	if(name != nil)
		u->name = runesdup(name);
	u->dom = nil;
	if(dom != nil)
		u->dom = runesdup(dom);

	u->next = users;
	users = u;

	return u;
}

/*
 * check if u is a member of gsid,
 * which might be a group.
 */
static int
ismembersid(Rune *srv, User *u, SID *gsid)
{
	User *g;

	if(EqualSid(u->sid, gsid))
		return 1;

	g = sidtouser(srv, gsid);
	if(g == 0)
		return 0;
	return ismember(u, g);
}

static int
ismember(User *u, User *g)
{
	Gmem *grps;

	if(EqualSid(u->sid, g->sid))
		return 1;

	if(EqualSid(g->sid, everyone))
		return 1;

	addgroups(u, 0);
	for(grps = u->group; grps != 0; grps = grps->next){
		if(EqualSid(grps->user->sid, g->sid)){
			return 1;
		}
	}
	return 0;
}

/*
 * find out what groups a user belongs to.
 * if force, throw out the old info and do it again.
 *
 * note that a global group is also know as a group,
 * and a local group is also know as an alias.
 * global groups can only contain users.
 * local groups can contain global groups or users.
 * this code finds all global groups to which a user belongs,
 * and all the local groups to which the user or a global group
 * containing the user belongs.
 */
static void
addgroups(User *u, int force)
{
	LOCALGROUP_USERS_INFO_0 *loc;
	GROUP_USERS_INFO_0 *grp;
	DWORD i, n, rem;
	User *gu;
	Gmem *g, *next;
	Rune *srv, srvrock[MAX_PATH];

	if(force){
		u->gotgroup = 0;
		for(g = u->group; g != nil; g = next){
			next = g->next;
			free(g);
		}
		u->group = nil;
	}
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
			g = malloc(sizeof(Gmem));
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
			g = malloc(sizeof(Gmem));
			g->user = gu;
			g->next = u->group;
			u->group = g;
		}
		net.ApiBufferFree(loc);
	}
}

static SID*
dupsid(SID *sid)
{
	SID *nsid;
	int n;

	n = GetLengthSid(sid);
	nsid = malloc(n);
	if(!CopySid(n, nsid, sid))
		fatal("can't copy sid");
	return nsid;
}

Rune*
pathcanonic(Rune *path, Rune *pathrock)
{
	Rune tmp[MAX_PATH+1], *last;
	Rune *p;
	int n;

	n = runeslen(path);
	if(n >= MAX_PATH) {
		printf("file name too long");
		return nil;
	}
	memmove(tmp, path, (n+1)*sizeof(Rune));

	for(p=tmp; *p; p++) {
		if(*p == '/')
			*p = '\\';
	}

	if(tmp[0] != 0 && tmp[1] == ':') {
		if(tmp[2] == 0) {
			tmp[2] = '\\';
			tmp[3] = 0;
		} else if(tmp[2] != '\\') {
			/* don't allow c:foo - only c:\foo */
			printf("illegal file name");
			return nil;
		}
	}

	path = pathrock;
	path[0] = 0;
	n = GetFullPathName(tmp, MAX_PATH, path, &last);
	if(n >= MAX_PATH) {
		printf("file name too long\n");
		return nil;
	}

	if(n == 0 && tmp[0] == '\\' && tmp[1] == '\\' && tmp[2] != 0) {
		memmove(path, tmp, (runeslen(tmp)+1)*sizeof(Rune));
		return path;
	}

	if(n == 0) {
		printf("bad file name\n");
		return nil;
	}

	for(p=path; *p; p++) {
		if(*p < 32 || *p == '*' || *p == '?') {
			printf("file not found\n");
			return nil;
		}
	}

	/* get rid of trailling \ */
	if(path[n-1] == '\\') {
		if(n <= 2) {
			printf("illegal file name\n");
			return nil;
		}
		path[n-1] = 0;
		n--;
	}
	return path;
}

/*
 * return the name of the server machine for file
 */
static Rune*
filesrv(Rune *file)
{
	int n;
	Rune *p, vol[3], uni[MAX_PATH];

	/* assume file is a fully qualified name - X: or \\server */
	if(file[1] == ':') {
		vol[0] = file[0];
		vol[1] = file[1];
		vol[2] = 0;
		if(GetDriveType(vol) != DRIVE_REMOTE)
			return 0;
		n = sizeof(uni);
		if(WNetGetUniversalName(vol, UNIVERSAL_NAME_INFO_LEVEL, uni, &n) != NO_ERROR)
			return nil;
		file = ((UNIVERSAL_NAME_INFO*)uni)->lpUniversalName;
	}
	file += 2;
	p = runeschr(file, '\\');
	if(p == 0)
		n = runeslen(file);
	else
		n = p - file;
	if(n >= MAX_PATH)
		n = MAX_PATH-1;

	memmove(uni, file, n*sizeof(Rune));
	uni[n] = '\0';

	return runesdup(uni);
}

/*
 * given a domain, find out the server to ask about its users.
 * we just ask the local machine to do the translation,
 * so it might fail sometimes.  in those cases, we don't
 * trust the domain anyway, and vice versa, so it's not
 * clear what benifit we would gain by getting the answer "right".
 */
static Rune*
domsrv(Rune *dom, Rune srv[MAX_PATH])
{
	Rune *psrv;
	int n, r;

	if(dom[0] == 0)
		return 0;

	r = net.GetAnyDCName(NULL, dom, (LPBYTE*)&psrv);
	if(r == NERR_Success) {
		n = runeslen(psrv);
		if(n >= MAX_PATH)
			n = MAX_PATH-1;
		memmove(srv, psrv, n*sizeof(Rune));
		srv[n] = 0;
		net.ApiBufferFree(psrv);
		return srv;
	}

	return 0;
}

void
secdump(Rune *file)
{
	SECURITY_INFORMATION si;
	SECURITY_DESCRIPTOR *sd;
	SECURITY_DESCRIPTOR_CONTROL sdctl;
	ACL *acl;
	SID *sid;
	BOOL hasacl, b;
	DWORD need, rev;
	Rune *srv;

	/*
	 * can't get SACL_SECURITY_INFORMATION:
	 * A required priveledge is not held by the client
	 */
	si = OWNER_SECURITY_INFORMATION
		| GROUP_SECURITY_INFORMATION
		| DACL_SECURITY_INFORMATION;
	GetFileSecurity(file, si, NULL, 0, &need);
	sd = malloc(need);
	if(!GetFileSecurity(file, si, sd, need, &need)){
		error("can't get security descriptor for %S: %d %x %s\n",
			file, need, sd, winerror());
		return;
	}

	srv = filesrv(file);

	if(!GetSecurityDescriptorControl(sd, &sdctl, &rev)){
		error("can't get security descriptor control: %s\n", winerror());
	}else{
		printf("security descriptor control: rev %d\n", rev);
		if(sdctl & SE_OWNER_DEFAULTED)
			printf("\towner defaulted\n");
		if(sdctl & SE_GROUP_DEFAULTED)
			printf("\tgroup defaulted\n");
		if(sdctl & SE_DACL_PRESENT)
			printf("\tdacl present\n");
		if(sdctl & SE_DACL_DEFAULTED)
			printf("\tdacl defaulted\n");
		if(sdctl & SE_SACL_PRESENT)
			printf("\tsacl present\n");
		if(sdctl & SE_SACL_DEFAULTED)
			printf("\tsacl defaulted\n");
		if(sdctl & SE_SELF_RELATIVE)
			printf("\tself relative\n");
	}
  
	/*
	 * need to dump sid from LookupAccountName(...GetUser...)
	 */
	if(!GetSecurityDescriptorOwner(sd, &sid, &b))
		error("can't get security descriptor owner: %s\n", winerror());
	else
		dumpsid(srv, "owner", sid);


	if(!GetSecurityDescriptorGroup(sd, &sid, &b))
		error("can't get security descriptor group: %s\n", winerror());
	else
		dumpsid(srv, "group", sid);

	if(!GetSecurityDescriptorDacl(sd, &hasacl, &acl, &b))
		error("can't get discretionary acl: %s\n", winerror());
	else if(!hasacl)
		printf("no discretionary acl, everything allowed\n");
	else
		dumpacl(srv, "discrectionary", acl);

	free(sd);
}

Valmap acetypemap[] =
{
	{ACCESS_ALLOWED_ACE_TYPE,	"allow"},
	{ACCESS_DENIED_ACE_TYPE,	"deny"},
};

Valmap aceflagsmap[] =
{
	{OBJECT_INHERIT_ACE,		"object inherit"},
	{CONTAINER_INHERIT_ACE,		"container inherit"},
	{NO_PROPAGATE_INHERIT_ACE ,	"no propagate inherit"},
	{INHERIT_ONLY_ACE,		"inherit only"},
};

Valmap accessmaskmap[] = 
{
	/*
	 * common
	 */
	{GENERIC_ALL,		"generic all"},
	{GENERIC_EXECUTE,	"generic exec"},
	{GENERIC_READ,		"generic read"},
	{GENERIC_WRITE,		"generic write"},
 
	{DELETE,		"delete"},
	{READ_CONTROL,		"read sec descriptor"},
	{SYNCHRONIZE,		"sync"},
	{WRITE_DAC,		"write disc acl"},
	{WRITE_OWNER,		"write owner"},
 
	/*
	 * file specific
	 */
	{FILE_READ_DATA,	"file read"},
	{FILE_WRITE_DATA,	"file write"},
	{FILE_APPEND_DATA,	"file append"},
	{FILE_READ_EA,		"file read ea"},
	{FILE_WRITE_EA,		"file write ea"},
	{FILE_EXECUTE,		"file execute"},
	{FILE_DELETE_CHILD,	"file delete child"},
	{FILE_READ_ATTRIBUTES,	"file read attributes"},
	{FILE_WRITE_ATTRIBUTES,	"file write attributes"},
  };

void
dumpacl(Rune *srv, char *name, ACL *acl)
{
	ACL_REVISION_INFORMATION rev;
	ACL_SIZE_INFORMATION size;
	VOID *vace;
	DWORD i;

	if(!GetAclInformation(acl, &size, sizeof(size), AclSizeInformation))
		return;
	printf("%s acl:\n", name);
	if(GetAclInformation(acl, &rev, sizeof(rev), AclRevisionInformation))
		printf("\trevision %d\n", rev.AclRevision);

	printf("\t%d access control entries\n", size.AceCount);
	for(i = 0; i < size.AceCount; i++){
		printf("\n");
		if(!GetAce(acl, i, &vace)){
			printf("can't get entry %d: %s\n", i, winerror());
			continue;
		}
		dumpace(srv, vace);
	}
}

void
dumpace(Rune *srv, void *vace)
{
	ACE_HEADER *aceh;
	ACCESS_ALLOWED_ACE *ace;

	aceh = vace;
	printf("\tkind: ");
	printvalmap(acetypemap, nelem(acetypemap), aceh->AceType);
	printf("\n\tflags: ");
	printvalflags(aceflagsmap, nelem(aceflagsmap), aceh->AceFlags);
	printf("\n");
	if(aceh->AceType != ACCESS_ALLOWED_ACE_TYPE
	&& aceh->AceType != ACCESS_DENIED_ACE_TYPE)
		return;

	ace = vace;
	printaccess(ace->Mask);
	dumpsid(srv, "\tto", (SID*)&ace->SidStart);
}

void
printaccess(ulong mask)
{
	printf("\taccess: ");
	printvalflags(accessmaskmap, nelem(accessmaskmap), mask);
	printf("\n");
}

Valmap ridmap[] =
{
	{SECURITY_DIALUP_RID,		"security dialup"},
	{SECURITY_NETWORK_RID,		"security network"},
	{SECURITY_BATCH_RID,		"security batch"},
	{SECURITY_INTERACTIVE_RID,	"security interactive"},
	{SECURITY_SERVICE_RID,		"security service"},
	{SECURITY_ANONYMOUS_LOGON_RID,	"security anonymous login"},
	{SECURITY_PROXY_RID,		"security proxy"},
	{SECURITY_SERVER_LOGON_RID,	"security server logon"},
	{SECURITY_LOCAL_SYSTEM_RID,	"security local system"},
	{SECURITY_NT_NON_UNIQUE,	"security nt non-unique"},
	{SECURITY_BUILTIN_DOMAIN_RID,	"security builtin domain"},

	/*
	 * well-known domain relative sub-authority values (RIDs)...
	 */
	{DOMAIN_USER_RID_ADMIN,		"domain admin"},
	{DOMAIN_USER_RID_GUEST,		"domain guest"},
	{DOMAIN_GROUP_RID_ADMINS,	"domain admins group"},
	{DOMAIN_GROUP_RID_USERS,	"domain users group"},
	{DOMAIN_GROUP_RID_GUESTS,	"domain guests group"},
	{DOMAIN_ALIAS_RID_ADMINS,	"domain admins alias"},
	{DOMAIN_ALIAS_RID_USERS,	"domain users alias"},
	{DOMAIN_ALIAS_RID_GUESTS,	"domain guests alisa"},
	{DOMAIN_ALIAS_RID_POWER_USERS,	"domain power users alias"},
	{DOMAIN_ALIAS_RID_ACCOUNT_OPS,	"domain account ops alias"},
	{DOMAIN_ALIAS_RID_SYSTEM_OPS,	"domain system ops alias"},
	{DOMAIN_ALIAS_RID_PRINT_OPS,	"domain print ops alias"},
	{DOMAIN_ALIAS_RID_BACKUP_OPS,	"domain backup ops alias"},
	{DOMAIN_ALIAS_RID_REPLICATOR,	"domain replicator alias"},
};

Valmap sidtypemap[] =
{
	{SidTypeUser,			"user"},
	{SidTypeGroup,			"group"},
	{SidTypeDomain,			"domain"},
	{SidTypeAlias,			"alias"},
	{SidTypeWellKnownGroup,		"well-known group"},
	{SidTypeDeletedAccount,		"deleted account"},
	{SidTypeInvalid,		"invalid"},
	{SidTypeUnknown,		"unknown"},
};

void
dumpsid(Rune *srv, char *name, SID *sid)
{
	SID_IDENTIFIER_AUTHORITY *sidida;
	Rune aname[100], dname[100];
	DWORD v, naname, ndname;
	SID_NAME_USE type;
	int i, n;
	static BYTE nulls[5];

	if(sid == NULL)
		return;
	sidida = GetSidIdentifierAuthority(sid);
	printf("%s:\n", name);
	if(sidida != NULL){
		if(memcmp(sidida->Value, nulls, 5) == 0){
			switch(sidida->Value[5]){
			case 0:
				printf("\tnull authority\n");
				break;
			case 1:
				printf("\tworld authority\n");
				break;
			case 2:
				printf("\tlocal authority\n");
				break;
			case 3:
				printf("\tcreator authority\n");
				break;
			case 4:
				printf("\tnon-unique authority\n");
				break;
			case 5:
				printf("\tnt authority\n");
				break;
			default:
				printf("identified authority 0x%x\n", sidida->Value[5]);
				break;
			}
		}else{
			printf("\tidentifier authority 0x%.2x%.2x%.2x%.2x%.2x%.2x\n",
				sidida->Value[0], sidida->Value[1], sidida->Value[2],
				sidida->Value[3], sidida->Value[4], sidida->Value[5]);
		}
	}
	n = *GetSidSubAuthorityCount(sid);
	for(i = 0; i < n; i++){
		v = *GetSidSubAuthority(sid, i);
		if(v == SECURITY_LOGON_IDS_RID){
			printf("\tsub logon 0x");
			i++;
			if(i < n)
				printf("%.2x", *GetSidSubAuthority(sid, i));
			i++;
			if(i < n)
				printf("%.2x", *GetSidSubAuthority(sid, i));
			printf("\n");
			continue;
		}

		printf("\tsub ");
		printvalmap(ridmap, nelem(ridmap), v);
		printf("\n");
	}

	naname = sizeof(aname);
	ndname = sizeof(dname);
	if(LookupAccountSid(srv, sid, aname, &naname, dname, &ndname, &type)){
		printf("\tdomain '%S' account '%S' type ", dname, aname);
		printvalmap(sidtypemap, nelem(sidtypemap), type);
		printf("\n");
	}
}

void
printvalmap(Valmap *vm, int n, int v)
{
	int i;

	for(i = 0; i < n; i++){
		if(vm[i].val == v){
			printf("%s", vm[i].name);
			return;
		}
	}
	printf("0x%x", v);
}

void
printvalflags(Valmap *vm, int n, int v)
{
	char *got;
	int i;

	printf("0x%x", v);
	got = " = ";
	for(i = 0; v && i < n; i++){
		if(vm[i].val & v){
			printf("%s%s", got, vm[i].name);
			got = " | ";
			v &= ~vm[i].val;
		}
	}
	if(v)
		printf("%s0x%x", got, v);
}

void
fatal(char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	vfprintf(stderr, fmt, arg);
	va_end(arg);
	exit(1);
}

void
error(char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	vfprintf(stderr, fmt, arg);
	va_end(arg);
}

int
runescmp(Rune *s, Rune *t)
{
	int c;

	while(c = *s){
		if(c != *t)
			break;
		s++;
		t++;
	}
	return c - *t;
}

int
runeslen(Rune *s)
{
	int n;

	for(n = 0; *s; s++)
		n++;
	return n;
}

Rune*
runeschr(Rune *s, int c)
{
	while(*s != c)
		if(*s++ == '\0')
			return nil;
	return s;
}

Rune*
runesdup(Rune *s)
{
	Rune *u;
	int n;

	n = (runeslen(s) + 1) * sizeof(Rune);
	u = malloc(n);
	if(u == nil)
		return nil;
	memmove(u, s, n);
	return u;
}

Rune*
utftorunes(char *s)
{
	Rune *u;
	int i, n;

	n = strlen(s) + 1;
	u = malloc(n * sizeof(Rune));
	if(u == nil)
		return nil;
	for(i = 0; i < n; i++)
		u[i] = s[i];
	return u;
}

char*
winerror(void)
{
	int e, r, i;
	static char buf[100];

	e = GetLastError();
	
	r = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
		0, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buf, sizeof(buf), 0);

	if(r == 0)
		sprintf(buf, "windows error %d", e);

	for(i = strlen(buf)-1; i>=0 && buf[i] == '\n' || buf[i] == '\r'; i--)
		buf[i] = 0;

	return buf;
}
