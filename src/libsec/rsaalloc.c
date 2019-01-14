#include "os.h"
#include <mp.h>
#include <libsec.h>

RSApub*
rsapuballoc(void)
{
	RSApub *rsa;

	rsa = malloc(sizeof(*rsa));
	if(rsa == nil)
		sysfatal("rsapuballoc");
	memset(rsa, 0, sizeof(*rsa));
	return rsa;
}

void
rsapubfree(RSApub *rsa)
{
	if(rsa == nil)
		return;
	mpfree(rsa->ek);
	mpfree(rsa->n);
}


RSApriv*
rsaprivalloc(void)
{
	RSApriv *rsa;

	rsa = malloc(sizeof(*rsa));
	if(rsa == nil)
		sysfatal("rsaprivalloc");
	memset(rsa, 0, sizeof(*rsa));
	return rsa;
}

void
rsaprivfree(RSApriv *rsa)
{
	if(rsa == nil)
		return;
	mpfree(rsa->pub.ek);
	mpfree(rsa->pub.n);
	mpfree(rsa->dk);
	mpfree(rsa->p);
	mpfree(rsa->q);
	mpfree(rsa->kp);
	mpfree(rsa->kq);
	mpfree(rsa->c2);
}
