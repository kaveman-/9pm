#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <mp.h>
#include <libsec.h>

#include "srx.h"

#define CRC(c) crc = crctab[(crc ^ (c)) & 0xff] ^ (crc >> 8)
/* This evaluates c only once */

ulong	crctab[256];


char *packet_names[] = {
	/*0*/ "SSH_MSG_NONE",
	/*1*/ "SSH_MSG_DISCONNECT",
	/*2*/ "SSH_SMSG_PUBLIC_KEY",
	/*3*/ "SSH_CMSG_SESSION_KEY",
	/*4*/ "SSH_CMSG_USER",
	/*5*/ "SSH_CMSG_AUTH_RHOSTS",
	/*6*/ "SSH_CMSG_AUTH_RSA",
	/*7*/ "SSH_SMSG_AUTH_RSA_CHALLENGE",
	/*8*/ "SSH_CMSG_AUTH_RSA_RESPONSE",
	/*9*/ "SSH_CMSG_AUTH_PASSWORD",
	/*10*/ "SSH_CMSG_REQUEST_PTY",
	/*11*/ "SSH_CMSG_WINDOW_SIZE",
	/*12*/ "SSH_CMSG_EXEC_SHELL",
	/*13*/ "SSH_CMSG_EXEC_CMD",
	/*14*/ "SSH_SMSG_SUCCESS",
	/*15*/ "SSH_SMSG_FAILURE",
	/*16*/ "SSH_CMSG_STDIN_DATA",
	/*17*/ "SSH_SMSG_STDOUT_DATA",
	/*18*/ "SSH_SMSG_STDERR_DATA",
	/*19*/ "SSH_CMSG_EOF",
	/*20*/ "SSH_SMSG_EXITSTATUS",
	/*21*/ "SSH_MSG_CHANNEL_OPEN_CONFIRMATION",
	/*22*/ "SSH_MSG_CHANNEL_OPEN_FAILURE",
	/*23*/ "SSH_MSG_CHANNEL_DATA",
	/*24*/ "SSH_MSG_CHANNEL_CLOSE",
	/*25*/ "SSH_MSG_CHANNEL_CLOSE_CONFIRMATION",
	/*27*/ "SSH_SMSG_X11_OPEN",
	/*28*/ "SSH_CMSG_PORT_FORWARD_REQUEST",
	/*29*/ "SSH_MSG_PORT_OPEN",
	/*30*/ "SSH_CMSG_AGENT_REQUEST_FORWARDING",
	/*31*/ "SSH_SMSG_AGENT_OPEN",
	/*32*/ "SSH_MSG_IGNORE",
	/*33*/ "SSH_CMSG_EXIT_CONFIRMATION",
	/*34*/ "SSH_CMSG_X11_REQUEST_FORWARDING",
	/*35*/ "SSH_CMSG_AUTH_RHOSTS_RSA",
	/*36*/ "SSH_MSG_DEBUG",
	/*37*/ "SSH_CMSG_REQUEST_COMPRESSION",
	/*38*/ "SSH_CMSG_MAX_PACKET_SIZE",
	/*39*/ "SSH_CMSG_AUTH_TIS",
	/*40*/ "SSH_SMSG_AUTH_TIS_CHALLENGE",
	/*41*/ "SSH_CMSG_AUTH_TIS_RESPONSE",
	/*42*/ "SSH_CMSG_AUTH_KERBEROS",
	/*43*/ "SSH_SMSG_AUTH_KERBEROS_RESPONSE",
	/*44*/ "SSH_CMSG_HAVE_KERBEROS_TGT"
};

void
preparekey(Connection *c) {
	uchar buf[16];

	switch (c->encryption) {
	case SSH_CIPHER_NONE:
		break;
	case SSH_CIPHER_IDEA:
		blFatal("Not implemented");
	case SSH_CIPHER_DES:
		setupDESstate(&c->inDESstate[0], c->key, nil);
		setupDESstate(&c->outDESstate[0], c->key, nil);
		break;
	case SSH_CIPHER_3DES:
		setupDESstate(&c->inDESstate[0], c->key, nil);
		setupDESstate(&c->outDESstate[0], c->key, nil);
		setupDESstate(&c->inDESstate[1], c->key+8, nil);
		setupDESstate(&c->outDESstate[1], c->key+8, nil);
		setupDESstate(&c->inDESstate[2], c->key+16, nil);
		setupDESstate(&c->outDESstate[2], c->key+16, nil);
		break;
	case SSH_CIPHER_TSS:
		blFatal("Not implemented");
	case SSH_CIPHER_RC4:
		setupRC4state(&c->rc4s2ckey, &c->key[0], 16);
		setupRC4state(&c->rc4c2skey, &c->key[16], 16);
		break;
	}
	genrandom(buf, sizeof(buf));
	setupRC4state(&c->rand, buf, sizeof(buf));
}

int
readn(Connection *c, uchar *buf, int n)
{
	int nn;

	while (n > 0) {
		nn = recv(c->socket, buf, n, 0);
		if(nn < 0) {
			blOSError("recv");
			return 0;
		}
		if(nn == 0) {
			blSetError("ssh", "unexpected eof");
			return 0;
		}
		buf += nn;
		n -= nn;
	}
	return 1;
}

Packet *
getpacket(Connection *c)
{
	ulong	l;
	int	n, i, pad;
	Packet	*packet;
	uchar	ll[4];
	uchar	*p;
	ulong	crc;

Again:
	if(!readn(c, ll, 4))
		return 0;

	l = (ll[0]<<24) | (ll[1]<<16) | (ll[2]<<8) | ll[3];

	if(l > 262144+8) {
		blSetError("ssh", "packet too large");
		return 0;
	}

	/* allocate packet */
	packet = (Packet *)blMalloc(sizeof(Packet) + l);

	pad = l & 7;
	packet->length = l - 5;	/* Subtract length of CRC and type */
	debug("Receiving packet, data length %lx, wire length %lx, padded length %lx\n",
		packet->length, l, packet->length + 13 - pad);
	l += 8 - pad;

	n = l;
	if(!readn(c, &packet->pad[pad], n)) {
		blFree(packet);
		return 0;
	}

	p = &packet->pad[pad];
	switch (c->encryption) {
	case SSH_CIPHER_NONE:
		break;
	case SSH_CIPHER_IDEA:
		blFatal("Not implemented");
	case SSH_CIPHER_DES:
		desCBCdecrypt(p, l, &c->inDESstate[0]);
		break;
	case SSH_CIPHER_3DES:
		desCBCdecrypt(p, l, &c->inDESstate[2]);
		desCBCencrypt(p, l, &c->inDESstate[1]);
		desCBCdecrypt(p, l, &c->inDESstate[0]);
		break;
	case SSH_CIPHER_TSS:
		blFatal("Not implemented");
	case SSH_CIPHER_RC4:
		if (c->role == Client)
			rc4(&c->rc4s2ckey, p, l);
		else
			rc4(&c->rc4c2skey, p, l);
		break;
	}

	/* crc of pad bytes */
	crc = 0;
	for(i = pad; i < 8; i++) {
		CRC(packet->pad[i]);
	}
	/* Calculate Remainder of CRC */
	CRC(packet->type);

	i = packet->length;
	p = packet->data;
	while (i--) CRC(*p++);

	l = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
	if (l != crc) {
		blSetError("ssh", "bad crc");
		blFree(packet);
		return 0;
	}

	if (packet->type == SSH_MSG_DEBUG) {
		blFree(packet);
		goto Again;
	}
	if (packet->type == 0) {
		blSetError("ssh", "illegal packet type");
		blFree(packet);
		return 0;
	}

	debug("Received %s Packet\n", packet_names[packet->type]);

	packet->pos = packet->data;
	return packet;
}

int
putpacket(Connection *c, Packet *packet)
{
	int	l;
	int	pad, i;
	uchar	*p;
	ulong	crc;

	l = packet->length + 5;	/* add type and CRC */

	/* create pad bytes */
	pad = l & 7;

	p = &(packet->pad[pad-4]);	/* Write length field */
	/* This partially overwrites packet->length!! */
	*p++ = (uchar)(l >> 24);
	*p++ = (uchar)(l >> 16);
	*p++ = (uchar)(l >>  8);
	*p   = (uchar)(l);

	debug("Sending %s Packet\n",packet_names[packet->type]);
	debug("Data length %lx, wire length %lx, pad length %lx\n", l - 5 , l, 8 - pad);

	crc = 0;
	if(c->encryption)
		rc4(&c->rand, &packet->pad[pad], 8-pad);
	else
		memset(&packet->pad[pad], 0, 8-pad);

	for(i = pad; i < 8; i++) {
		CRC(packet->pad[i]);
	}
	/* Calculate Remainder of CRC */
	CRC(packet->type);

	i = l - 5;
	p = packet->data;
	while (i--) CRC(*p++);

	*p++ = (uchar)(crc >> 24);
	*p++ = (uchar)(crc >> 16);
	*p++ = (uchar)(crc >>  8);
	*p   = (uchar)crc;

	l += 8 - pad;

	debug("CRC = 0x%lux\n", crc);

	p = &packet->pad[pad];
	switch (c->encryption) {
	case SSH_CIPHER_NONE:
		break;
	case SSH_CIPHER_IDEA:
		blFatal("Not implemented");
	case SSH_CIPHER_DES:
		desCBCencrypt(p, l, &c->outDESstate[0]);
		break;
	case SSH_CIPHER_3DES:
		desCBCencrypt(p, l, &c->outDESstate[0]);
		desCBCdecrypt(p, l, &c->outDESstate[1]);
		desCBCencrypt(p, l, &c->outDESstate[2]);
		break;
	case SSH_CIPHER_TSS:
		blFatal("Not implemented");
	case SSH_CIPHER_RC4:
		if (c->role == Server)
			rc4(&c->rc4s2ckey, p, l);
		else
			rc4(&c->rc4c2skey, p, l);
		break;
	}
	if (send(c->socket, &packet->pad[pad-4], l+4, 0) != l+4) {
		blFree(packet);
		return 0;
	}
	blFree(packet);
	return 1;
}

void
mkcrctab(ulong poly)
{
	ulong crc;
	int i, j;

	for(i = 0; i < 256; i++){
		crc = i;
		for(j = 0; j < 8; j++){
			if(crc & 1)
				crc = (crc >> 1) ^ poly;
			else
				crc >>= 1;
		}
		crctab[i] = crc;
	}
}

RSApub *
getRSApublic(Packet *packet) {
	RSApub *key;
	ulong skl;

	skl = getlong(packet);
	USED(skl);
	key = rsapuballoc();
	key->ek = getBigInt(packet);
	key->n = getBigInt(packet);
	return key;
}

ulong
getlong(Packet *packet) {
	ulong x;

	if (packet->length < 4) blFatal("Packet too small");
	packet->length -= 4;
	x =            *(packet->pos)++;
	x = (x << 8) | *(packet->pos)++;
	x = (x << 8) | *(packet->pos)++;
	x = (x << 8) | *(packet->pos)++;
	return x;
}

ushort
getshort(Packet *packet) {
	ushort x;

	if ((packet->length -= 2) < 0) blFatal("Packet too small");
	x =            *(packet->pos)++;
	x = (x << 8) | *(packet->pos)++;
	return x;
}

uchar
getbyte(Packet *packet) {

	if (packet->length-- == 0) blFatal("Packet too small");
	return *(packet->pos)++;
}

void
getbytes(Packet *packet, uchar *buf, int n) {

	if ((packet->length -= n) < 0) blFatal("Packet too small");
	while (n--) *buf++ = *(packet->pos)++;
}

int
getstring(Packet *packet, char *s, int n) {
	int l;

	l = getlong(packet);
	if (l >= n)
		blFatal("Insufficient length for string, need %d, have %d", l, n);
	getbytes(packet, (uchar *)s, l);
	s[l] = 0;
	return l;
}

void
putstring(Packet *packet, char *s, int n) {

	putlong(packet, n);
	putbytes(packet, (uchar *)s, n);
}

mpint*
getBigInt(Packet *packet) {
	ushort bytes;
	uchar buf[1024];

	bytes = (getshort(packet)+7) >> 3;
	getbytes(packet, buf, bytes);
	return betomp(buf, bytes, nil);
}

void
putlong(Packet *packet, ulong x) {

	packet->length += 4;
	*(packet->pos)++ = (uchar)(x >> 24);
	*(packet->pos)++ = (uchar)(x >> 16);
	*(packet->pos)++ = (uchar)(x >>  8);
	*(packet->pos)++ = (uchar)x;
}

void
putshort(Packet *packet, ushort x) {

	packet->length += 2;
	*(packet->pos)++ = (uchar)(x >>  8);
	*(packet->pos)++ = (uchar)(x);
}

void
putbyte(Packet *packet, uchar x) {

	packet->length++;
	*(packet->pos)++ = x;
}

void
putbytes(Packet *packet, uchar *buf, int n) {

	packet->length += n;
	memmove(packet->pos, buf, n);
	packet->pos += n;
}

void
putBigInt(Packet *packet, mpint *b) {
	ushort bits, bytes;
	uchar buf[1024];

	bits = mpsignif(b);
	bytes = mptobe(b, buf, 1024, nil);
	putshort(packet, bits);
	putbytes(packet, buf, bytes);
if((bits+7)/8 != bytes) blFatal("putBigInt %B(%d) %d bits, %d bytes", b, b->top, bits, bytes);
}

void
putRSApublic(Packet *packet, RSApub *key) {

	putlong(packet, mpsignif(key->n));
	putBigInt(packet, key->ek);
	putBigInt(packet, key->n);
}
