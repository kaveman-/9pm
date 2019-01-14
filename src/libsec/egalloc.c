#include "os.h"
#include <mp.h>
#include <libsec.h>

EGpub*
egpuballoc(void)
{
	EGpub *eg;

	eg = malloc(sizeof(*eg));
	if(eg == nil)
		sysfatal("egpuballoc");
	memset(eg, 0, sizeof(*eg));
	return eg;
}

void
egpubfree(EGpub *eg)
{
	if(eg == nil)
		return;
	mpfree(eg->p);
	mpfree(eg->alpha);
	mpfree(eg->key);
}


EGpriv*
egprivalloc(void)
{
	EGpriv *eg;

	eg = malloc(sizeof(*eg));
	if(eg == nil)
		sysfatal("egprivalloc");
	memset(eg, 0, sizeof(*eg));
	return eg;
}

void
egprivfree(EGpriv *eg)
{
	if(eg == nil)
		return;
	mpfree(eg->pub.p);
	mpfree(eg->pub.alpha);
	mpfree(eg->pub.key);
	mpfree(eg->secret);
}

EGsig*
egsigalloc(void)
{
	EGsig *eg;

	eg = malloc(sizeof(*eg));
	if(eg == nil)
		sysfatal("egsigalloc");
	memset(eg, 0, sizeof(*eg));
	return eg;
}

void
egsigfree(EGsig *eg)
{
	if(eg == nil)
		return;
	mpfree(eg->r);
	mpfree(eg->s);
}
