#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <mp.h>
#include <libsec.h>

#include "srx.h"

void usage(void);
void init(void);
int dial(char *host, int port);
char *buildargs(char *argv[]);
void threadInput(long);
BOOL WINAPI handler(DWORD a);
void runcmd(Connection *c, char *cmd);

enum {
	SshPort = 22,
};

char *argv0;
int oflag;
int dflag = 0;

int keyexchange(Connection *c);

void
main(int argc, char *argv[])
{
	char *cmd;
	Connection *c;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	setmode( _fileno( stdin ), _O_BINARY );
	setmode( _fileno( stdout ), _O_BINARY );

	ARGBEGIN {
	default:
		usage();
	case 'o':
		oflag++;
		break;
	case 'i':
		SetConsoleCtrlHandler(handler, 1);
		break;
	case 'd':
		dflag++;
		break;
	} ARGEND

	if(argc < 2)
		usage();

	init();
	c = blMallocZ(sizeof(Connection));
	c->role = Client;
	c->socket = dial(argv[0], SshPort);
	if(c->socket < 0)
		blFatal("could not dial: %s: %s", argv[0], blGetErrorMessage());

	if(!keyexchange(c))
		blFatal("keyexchange failed: %s", blGetError());
	if(!auth(c))
		blFatal("auth failed: %s", blGetError());

	cmd = buildargs(argv+1);
	runcmd(c, cmd);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-oid] host cmd ...\n", argv0);
	exit(1);
}

void
init(void)
{
	WSADATA wsadat;

	blAttach();
	if(WSAStartup(MAKEWORD(2, 0), &wsadat) != 0) {
		blOSError("WSAStartup");
		blFatal("WSAStartup failed: %s", blGetErrorMessage());
	}

	mkcrctab(0xedb88320);
}

int
keyexchange(Connection *c)
{
	char buf[512], *p;
	int i, n;

	/* First send ID string */
	debug("Client version: %s%s\n", PROTSTR, PROTVSN);
	for (i = 0;;i++)  {
		n = recv(c->socket, &buf[i], 1, 0);
		if(n < 0) {
			blOSError("recv");
			return 0;
		}
		if(n == 0) {
			blSetError("ssh", "unexpected eof");
			return 0;
		}
		if (buf[i] == '\n') break;
		if (i >= 256) {
			blSetError("ssh", "server version string too long");
			return 0;
		}
	}
	buf[i] = 0;
	debug("Server version: %s\n", buf);

	/* pick apart the server id: SSH-m.n-comment.  We require m=1, n>=5 */
	if (strncmp(buf, "SSH-", 4) != 0
	|| strtol(buf+4, &p, 10) != 1 || *p != '.' || strtol(p+1, &p, 10) < 5 || *p != '-') {
	   	blSetError("ssh", "protocol mismatch");
		return 0;
	}
	n = sprintf(buf, "%s%s\n", PROTSTR, PROTVSN);
	if(send(c->socket, buf, n, 0) < n) {
		blOSError("send");
		return 0;
	}
	get_ssh_smsg_public_key(c);

	/* check key here */

	comp_sess_id(c);

	debug("Session ID: ");
	for (i = 0; i < 16; i++) debug("%2.2x", c->id[i]);
	debug("\n");

	/* Create session key */
	genrandom(c->key, sizeof(c->key));
	
	debug("Session key is ");
	for (i = 0; i < 32; i++)
		debug("%2.2x", c->key[i]);
	debug("\n");

	if (c->cipher_mask & (1 << SSH_CIPHER_RC4)) {
		c->cipher = SSH_CIPHER_RC4;
		debug("Using RC4 for data encryption\n");
	} else if (c->cipher_mask & (1 << SSH_CIPHER_3DES)) {
		c->cipher = SSH_CIPHER_3DES;
		debug("Using Triple-DES for data encryption\n");
	} else
		blFatal("Can't agree on ciphers: 0x%x", c->cipher_mask);

	put_ssh_cmsg_session_key(c);
	c->encryption = c->cipher;
	preparekey(c);
	if(!get_ssh_smsg_success(c))
		blFatal("key exchange failed");
	debug("Connection established\n");
	return 1;
}

/* Get the SSH_SMSG_PUBLIC_KEY message */
void
get_ssh_smsg_public_key(Connection *c)
{
	Packet *packet;
	int i;

	packet = getpacket(c);
	if (packet == 0)
		blFatal("Connection lost: %s", blGetError());
	if (packet->type != SSH_SMSG_PUBLIC_KEY)
		blFatal("Wrong message, expected %d, got %d",
			SSH_SMSG_PUBLIC_KEY, packet->type);
	if ((packet->length -= 8) < 0)
		blFatal("Packet too small");
	memcpy(c->cookie, packet->data, 8);
	packet->pos += 8;
	c->session_key = getRSApublic(packet);
	c->host_key = getRSApublic(packet);

	debug("Receiving session key ");
	debugbig(c->session_key->ek, ' ');
	debugbig(c->session_key->n, '\n');
	debug("Receiving host key ");
	debugbig(c->host_key->ek, ' ');
	debugbig(c->host_key->n, '\n');

	c->protocol_flags = getlong(packet);
	debug("Protocol flags %10lx\n", c->protocol_flags);	
	c->cipher_mask = getlong(packet);

	debug("Supported ciphers: %x\n", c->cipher_mask);
	for (i = 0; cipher_names[i]; i++)
		if ((1 << i) & c->cipher_mask)
			debug("\t%s\n", cipher_names[i]);

	c->authentications_mask = getlong(packet);

	debug("Supported authentication methods: %x\n", c->authentications_mask);
	for (i = 0; auth_names[i]; i++)
		if ((1 << i) & c->authentications_mask)
			debug("\t%s\n", auth_names[i]);
	debug("Remaining Length: %d\n", packet->length);

	blFree(packet);
}

void
comp_sess_id(Connection *c)
{
	uchar sess_str[1024];
	int bytes;

	bytes = mptobe(c->host_key->n, sess_str, sizeof(sess_str)-8, nil);
	bytes += mptobe(c->session_key->n, sess_str + bytes, sizeof(sess_str)-8-bytes, nil);

	memcpy(sess_str + bytes, c->cookie, 8);
	md5(sess_str, bytes+8, c->id, 0);
}

static mpint*
rsa_encrypt_buf(uchar *buf, int n, RSApub *key)
{
	int m;
	mpint *b;

	m = (mpsignif(key->n)+7)/8;
	RSApad(buf, n, buf, m);
	b = betomp(buf, m, nil);
	debug("padded %B\n", b);
	b = RSAEncrypt(b, key);
	debug("encrypted %B\n", b);
	return b;
}

/* Send the SSH_CMSG_SESSION_KEY message */
void
put_ssh_cmsg_session_key(Connection *c)
{
	Packet *packet;
	RSApub *kbig, *ksmall;
	mpint *b;
	int i, n, session_key_len, host_key_len;
	uchar *buf;

	packet = (Packet *)blMalloc(2048);
	packet->type = SSH_CMSG_SESSION_KEY;
	packet->length = 0;
	packet->pos = packet->data;

	putbyte(packet, c->cipher);
	debug("Cipher type is %s\n", cipher_names[c->cipher]);	
	putbytes(packet, c->cookie, 8);

	session_key_len = mpsignif(c->session_key->n);
	host_key_len = mpsignif(c->host_key->n);

	ksmall = kbig = nil;
	if(session_key_len+128 <= host_key_len) {
		ksmall = c->session_key;
		kbig = c->host_key;
	} else if(host_key_len+128 <= session_key_len) {
		ksmall = c->host_key;
		kbig = c->session_key;
	} else
		blFatal("Server session and host keys must differ by at least 128 bits\n");

	n = (mpsignif(kbig->n)+7)/8;
	buf = blMallocZ(n);

	memmove(buf, c->key, SESSION_KEY_LENGTH);
	for(i = 0; i < SESSION_ID_LENGTH; i++)
		buf[i] ^= c->id[i];

	b = rsa_encrypt_buf(buf, SESSION_KEY_LENGTH, ksmall);
	n = (mpsignif(ksmall->n)+7) / 8;
	mptobuf(b, buf, n);
	mpfree(b);

	b = rsa_encrypt_buf(buf, n, kbig);
	putBigInt(packet, b);
	mpfree(b);

	n = (mpsignif(kbig->n)+7)/8;
	memset(buf, 0, n);
	blFree(buf);

	putlong(packet, 0);	/* Protocol flags */

	putpacket(c, packet);
}

int
get_ssh_smsg_success(Connection *c)
{
	Packet *packet;
	int r;

	packet = getpacket(c);
	if (packet == 0)
		blFatal("Connection lost: %s", blGetError());
	r = packet->type == SSH_SMSG_SUCCESS;
	blFree(packet);
	return r;
}


void
runcmd(Connection *c, char *cmd)
{
	Packet *packet;
	uchar *buf, *p;
	int nbuf, n;
	

	packet = (Packet*)blMalloc(sizeof(Packet)+strlen(cmd)+64);
	packet->length = 0;
	packet->pos = packet->data;
	packet->type = SSH_CMSG_EXEC_CMD;
	putstring(packet, cmd, strlen(cmd));
	debug("sending cmd: %s\n", cmd);
	putpacket(c, packet);

	blThreadCreate(threadInput, (long)c);
	
	buf = 0;
	nbuf = 0;
	for(;;) {
		packet = getpacket(c);
		if(packet == 0)
			break;
		if((int)packet->length > nbuf){
			/*
			 * The smallest allowed maximum we can place on message length
			 * is a quarter megabyte, so we grow the buffer as necessary
			 * rather than allocate a quarter meg right off.
			 */
			nbuf = packet->length + 64;
			buf = blRealloc(buf, nbuf);
		}
		switch (packet->type) {
		case SSH_SMSG_EXITSTATUS:
			debug("Exit\n");
			blExit(0);
		case SSH_MSG_DISCONNECT:
			getstring(packet, buf, nbuf);
			debug("Exits(%s)\n", buf);
			blExit(1);
		case SSH_SMSG_STDOUT_DATA:
			p = packet->data;
			debug("Stdout pkt %d len %d\n", packet->length, (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3]);
			n = getstring(packet, buf, nbuf);
			write(1, buf, n);
			break;
		case SSH_SMSG_STDERR_DATA:
			n = getstring(packet, buf, nbuf);
			write(2, buf, n);
			break;
		default:
			blFatal("Unknown message type %d", packet->type);
		}
		blFree(packet);
	}
}

static int
msgwrite(Connection *c, char *buf, int n)
{
	Packet *packet;

	packet = (Packet *)blMalloc(sizeof(Packet)+8+n);
	packet->length = 0;
	packet->pos = packet->data;
	if (n == 0) {
		packet->type = SSH_CMSG_EOF;
		putpacket(c, packet);
		return 0;
	}
	packet->type = SSH_CMSG_STDIN_DATA;
	putstring(packet, buf, n);
	if (!putpacket(c, packet))
		return -1;
	return n;
}

void
threadInput(long a)
{
	int n;
	char buf[1024];
	Connection *c = (Connection*)(long)a;

	debug("threadInput started\n");

	for(;;){
		n = read(0, buf, sizeof(buf));
		if(n <= 0)
			break;

		if(msgwrite(c, buf, n) != n)
			break;
	}

	msgwrite(c, "", 0);
	blExit(0);
}

BOOL WINAPI
handler(DWORD a)
{
	return 1;
}


int
dial(char *host, int port)
{
	int so;
	struct sockaddr_in sin;
	struct hostent *hp;

	so = socket(AF_INET, SOCK_STREAM, 0);
	if(so == INVALID_SOCKET) {
		blOSError("socket");
		return -1;
	}
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	hp = gethostbyname(host);
	if(hp == 0){
		blOSError("gethostbyname");
		closesocket(so);
		return -1;
	}
	memmove((char*)&sin.sin_addr.s_addr, hp->h_addr_list[0], hp->h_length);
	if(connect(so, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
		blOSError("connect");
		closesocket(so);
		return -1;
	}
	return so;
}

char *
buildargs(char *argv[])
{
	char *args;
	int m, n;

	args = blMalloc(1);
	args[0] = '\0';
	n = 0;
	while(*argv){
		m = strlen(*argv) + 1;
		args = blRealloc(args, n+m +1);
		args[n] = ' ';	/* smashes old null */
		strcpy(args+n+1, *argv);
		n += m;
		argv++;
	}
	return args;
}

void
debug(char *fmt, ...)
{
	va_list arg;

	if(!dflag)
		return;

	va_start(arg, fmt);
	vfprintf(stderr, fmt, arg);
	fflush(stderr);
	va_end(arg);
}

void
debugbig(mpint *b, char term)
{
	uchar buf[2048];
	int s, i;

	if(!dflag)
		return;

	s = mptobe(b, buf, 1024, nil);
	for (i = 0; i < s; i++)
		fprintf(stderr, "%2.2x", buf[i]);
	if (term)
		fprintf(stderr, "%c", term);
}
