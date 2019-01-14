#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <mp.h>
#include <libsec.h>

#include "srx.h"

char	*cipher_names[] = {
	"No encryption",
	"IDEA",
	"DES",
	"Triple DES",
	"TSS",
	"RC4",
	"Blowfish",
	0
};

char	*auth_names[] = {
	"Illegal",
	".rhosts or /etc/hosts.equiv",
	"pure RSA",
	"Password",
	".shosts with RSA",
	"TIS",
	0
};

void
RSApad(uchar *inbuf, int inbytes, uchar *outbuf, int outbytes) {
	uchar *p, *q;
	uchar x;

	debug("Padding a %d-byte number to %d bytes\n", inbytes, outbytes);

	assert(outbytes >= inbytes+3);
	p = &inbuf[inbytes-1];
	q = &outbuf[outbytes-1];
	while (p >= inbuf) *q-- = *p--;
	*q-- = 0;
	while (q >= outbuf+2) {
		genrandom(&x, 1);
		if(x == 0)
			x = 1;
		*q-- = x;
	}
	outbuf[0] = 0;
	outbuf[1] = 2;
}

mpint*
RSAunpad(mpint *bin, int bytes) {
	int n;
	uchar buf[256];
	mpint *bout;

	bout = mpnew(0);
	n = mptobe(bin, buf, 256, nil);
	if (buf[0] != 2)
		blFatal("RSAunpad");
	betomp(buf+n-bytes, bytes, bout);
	return bout;
}

mpint*
RSAEncrypt(mpint *in, RSApub *rsa)
{
	mpint *out;

	out = rsaencrypt(rsa, in, nil);
	mpfree(in);
	return out;
}

mpint*
RSADecrypt(mpint *in, RSApriv *rsa)
{
	mpint *out;

	out = rsadecrypt(rsa, in, nil);
	mpfree(in);
	return out;
}

/*
 *  convert b into a big-endian array of bytes, right justified in buf
 */
void
mptobuf(mpint *b, uchar *buf, int len)
{
	int n;

	n = mptobe(b, buf, len, nil);
	assert(n >= 0);
	if(n < len){
		len -= n;
		memmove(buf+len, buf, n);
		memset(buf, 0, len);
	}
}
