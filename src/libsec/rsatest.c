#include "os.h"
#include <mp.h>
#include <libsec.h>

void
main(void)
{
	RSApriv *rsa;
	int n;
	mpint *clr, *enc, *clr2;

	rsa = rsagen(1024, 16, 0);
	if(rsa == nil)
		sysfatal("rsagen");
	clr = mpnew(0);
	clr2 = mpnew(0);
	enc = mpnew(0);

	strtomp("123456789abcdef123456789abcdef123456789abcdef123456789abcdef", nil, 16, clr);
	rsaencrypt(&rsa->pub, clr, enc);
	
	for(n = 0; n < 10; n++)
		rsadecrypt(rsa, enc, clr);

	for(n = 0; n < 10; n++)
		mpexp(enc, rsa->dk, rsa->pub.n, clr2);

	if(mpcmp(clr, clr2) != 0)
		printf("%s != %s\n", mptoa(clr,0,0,0), mptoa(clr2,0,0,0));
}
