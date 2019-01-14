/* disable various silly warnings */
#pragma warning( disable : 4305 4244 4102 4761 4164 )

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

#define nil 0

char *argv0;
#define	ARGBEGIN	for((argv0? 0: (argv0=*argv)),argv++,argc--;\
			    argv[0] && argv[0][0]=='-' && argv[0][1];\
			    argc--, argv++) {\
				char *_args, *_argt=0;\
				char _argc;\
				_args = &argv[0][1];\
				if(_args[0]=='-' && _args[1]==0){\
					argc--; argv++; break;\
				}\
				_argc = 0;\
				while(*_args && (_argc = *_args++))\
				switch(_argc)
#define	ARGEND		USED(_argt);}
#define	ARGF()		(_argt=_args, _args="",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): 0))
#define	ARGC()		_argc

#define USED(x)		if(x)

/* File names */
#define HOSTKEYPRIV	"/sys/lib/ssh/hostkey.secret"
#define HOSTKEYPUB	"/sys/lib/ssh/hostkey.public"

/*
Debugging only:
#define HOSTKEYPRIV	"./hostkey.secret"
#define HOSTKEYPUB	"./hostkey.public"
 */

#define HOSTKEYRING	"/sys/lib/ssh/keyring"

#define PROTSTR	"SSH-1.5-"
#define PROTVSN	"BowlineSSH"


#define DBG			0x01
#define DBG_CRYPTO	0x02
#define DBG_PACKET	0x04
#define DBG_AUTH	0x08
#define DBG_PROC	0x10
#define DBG_PROTO	0x20
#define DBG_IO		0x40
#define DBG_SCP		0x80

extern char *progname;

#define SSH_MSG_NONE							0
#define SSH_MSG_DISCONNECT						1
#define SSH_SMSG_PUBLIC_KEY						2
#define SSH_CMSG_SESSION_KEY					3
#define SSH_CMSG_USER							4
#define SSH_CMSG_AUTH_RHOSTS					5
#define SSH_CMSG_AUTH_RSA						6
#define SSH_SMSG_AUTH_RSA_CHALLENGE				7
#define SSH_CMSG_AUTH_RSA_RESPONSE				8
#define SSH_CMSG_AUTH_PASSWORD					9
#define SSH_CMSG_REQUEST_PTY					10
#define SSH_CMSG_WINDOW_SIZE					11
#define SSH_CMSG_EXEC_SHELL						12
#define SSH_CMSG_EXEC_CMD						13
#define SSH_SMSG_SUCCESS						14
#define SSH_SMSG_FAILURE						15
#define SSH_CMSG_STDIN_DATA						16
#define SSH_SMSG_STDOUT_DATA					17
#define SSH_SMSG_STDERR_DATA					18
#define SSH_CMSG_EOF							19
#define SSH_SMSG_EXITSTATUS						20
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION		21
#define SSH_MSG_CHANNEL_OPEN_FAILURE			22
#define SSH_MSG_CHANNEL_DATA					23
#define SSH_MSG_CHANNEL_CLOSE					24
#define SSH_MSG_CHANNEL_CLOSE_CONFIRMATION		25
/*	(OBSOLETED; was unix-domain X11 forwarding)	26 */
#define SSH_SMSG_X11_OPEN						27
#define SSH_CMSG_PORT_FORWARD_REQUEST			28
#define SSH_MSG_PORT_OPEN						29
#define SSH_CMSG_AGENT_REQUEST_FORWARDING		30
#define SSH_SMSG_AGENT_OPEN						31
#define SSH_MSG_IGNORE							32
#define SSH_CMSG_EXIT_CONFIRMATION				33
#define SSH_CMSG_X11_REQUEST_FORWARDING			34
#define SSH_CMSG_AUTH_RHOSTS_RSA				35
#define SSH_MSG_DEBUG							36
#define SSH_CMSG_REQUEST_COMPRESSION			37
#define SSH_CMSG_MAX_PACKET_SIZE				38
#define SSH_CMSG_AUTH_TIS						39
#define SSH_SMSG_AUTH_TIS_CHALLENGE				40
#define SSH_CMSG_AUTH_TIS_RESPONSE				41
#define SSH_CMSG_AUTH_KERBEROS					42
#define SSH_SMSG_AUTH_KERBEROS_RESPONSE			43
#define SSH_CMSG_HAVE_KERBEROS_TGT				44

#define SSH_CIPHER_NONE							0
#define SSH_CIPHER_IDEA							1
#define SSH_CIPHER_DES							2
#define SSH_CIPHER_3DES							3
#define SSH_CIPHER_TSS							4
#define SSH_CIPHER_RC4							5

extern char	*cipher_names[];

#define SSH_AUTH_RHOSTS							1
#define SSH_AUTH_RSA							2
#define SSH_AUTH_PASSWORD						3
#define SSH_AUTH_RHOSTS_RSA						4
#define SSH_AUTH_TIS							5
#define SSH_AUTH_USER_RSA						6

extern char	*auth_names[];

#define SSH_PROTOCOL_SCREEN_NUMBER				1
#define SSH_PROTOCOL_HOST_IN_FWD_OPEN			2
#define SSH_MAX_DATA	262144	/* smallest allowed maximum packet size */
#define SSH_MAX_MSG	SSH_MAX_DATA+4

#define SESSION_KEY_LENGTH	32
#define SESSION_ID_LENGTH 	MD5dlen

/* Used by both */


typedef struct Packet {
	uchar	*pos;	/* position in packet (while reading or writing */
	ulong	length;	/* output: #bytes before pos, input: #bytes after pos */
	uchar	pad[8];
	uchar	type;
	uchar	data[1];
} Packet;

typedef struct Connection {
	int socket;
	int role;
	int encryption;
	int compression;
	ulong protocol_flags;
	ulong cipher_mask;
	ulong authentications_mask;
	uchar key[SESSION_KEY_LENGTH];
	uchar id[SESSION_ID_LENGTH];
	uchar cipher;
	RSApub *session_key;
	RSApub *host_key;
	uchar cookie[8];
	DESstate inDESstate[3], outDESstate[3];
	RC4state rc4c2skey, rc4s2ckey;
	RC4state rand;
} Connection;


extern char		passwd[256];

enum {
	Client, Server
};

enum {
	key_ok, key_wrong, key_notfound, key_file
};

/* misc.c */
void			debug(char *, ...);
void			debugbig(mpint*, char);
void			printbig(int, mpint*, int, char);
void			printpublickey(int, RSApub *);
void			printdecpublickey(int, RSApub *);
void			printsecretkey(int, RSApriv *);
int			readsecretkey(FILE *, RSApriv *);
int			readpublickey(FILE *, RSApub *);
void			comp_sess_id(Connection *c);
void			RSApad(uchar *, int, uchar *, int);
mpint*			RSAunpad(mpint*, int);
mpint*			RSAEncrypt(mpint*, RSApub*);
mpint*			RSADecrypt(mpint*, RSApriv*);
int			verify_key(char *, RSApub *, char *);
int			replace_key(char *, RSApub *, char *);
void			getdomainname(char*, char*, int);
void			mptobuf(mpint*, uchar*, int);

/* packet.c */
void			mkcrctab(ulong);
Packet*			getpacket(Connection*);
int			putpacket(Connection*, Packet *);
void			preparekey(Connection*);
void			mfree(Packet *);

ulong			getlong(Packet *);
RSApub*			getRSApublic(Packet *);
ulong			getlong(Packet *);
ushort			getshort(Packet *);
uchar			getbyte(Packet *);
void			getbytes(Packet *, uchar *, int);
mpint*			getBigInt(Packet *);
int			getstring(Packet *, char *, int);

void			putlong(Packet *, ulong);
void			putshort(Packet *, ushort);
void			putbyte(Packet *, uchar);
void			putbytes(Packet *, uchar *, int);
void			putBigInt(Packet *, mpint*);
void			putRSApublic(Packet *, RSApub *);
void			putstring(Packet *, char *, int);


/* messages.c */
void			get_ssh_smsg_public_key(Connection *);
void			put_ssh_cmsg_session_key(Connection *c);
int			get_ssh_smsg_success(Connection *c);
void			user_auth(void);
void			run_shell(char *argv[]);
void			request_pty(void);

int			auth(Connection *c);

/* bowline stuff */
long blThreadCreate(void(*f)(long), long);
int blThreadSwitchCount(void);
void blThreadExit(void);
long blThreadId(void);
double blThreadTime(void);
void blSleep(long);
double blRealTime(void);
void blAttach(void);
void blDetach(void);
int blTlsAlloc(void);
void blTlsFree(int);
void blTlsSet(int, void*);
void *blTlsGet(int);

// error stuff
char *blGetError(void);
char *blGetErrorType(void);
char *blGetErrorMessage(void);
char *blOSError(char*);
void blSetError(char*, char*);
void blSetErrorEx(char*, char*, char*);
void blClearError(void);

// util
void blFatal(char *, ...);
void blExit(int);

// memory
void *blMalloc(int size);
void *blMallocZ(int size);
void *blRealloc(void*, int size);
void blFree(void*);
char *blStrDup(char *);

/* Used by the client */

extern RSApriv		*client_session_key;
extern RSApriv		*client_host_key;

extern char		*user;
extern char		*localuser;
extern char		server_host_name[];

extern int		 no_secret_key;
extern int		 verbose;
extern int		 crstrip;
extern int		interactive;
extern int		alwaysrequestpty;
extern int		cooked;
extern int		usemenu;

