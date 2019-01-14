/////////////////////////////////////////////////////////
// DES definitions
/////////////////////////////////////////////////////////

enum
{
	DESbsize=	8,
};

// single des
typedef struct DESstate DESstate;
struct DESstate
{
	unsigned long	setup;
	unsigned char	key[8];		/* unexpanded key */
	unsigned long	expanded[32];	/* expanded key */
	unsigned char	ivec[8];	/* initialization vector */
};

void	setupDESstate(DESstate *s, unsigned char key[8], unsigned char *ivec);
void	des_key_setup(unsigned char[8], unsigned long[32]);
void	block_cipher(unsigned long*, unsigned char*, int);
void	desCBCencrypt(unsigned char*, int, DESstate*);
void	desCBCdecrypt(unsigned char*, int, DESstate*);
void	desECBencrypt(unsigned char*, int, DESstate*);
void	desECBdecrypt(unsigned char*, int, DESstate*);

// for backward compatibility with 7 byte DES key format
void	des56to64(unsigned char *k56, unsigned char *k64);
void	des64to56(unsigned char *k64, unsigned char *k56);
void	key_setup(unsigned char[7], unsigned long[32]);

// triple des encrypt/decrypt orderings
enum {
	DES3E=		0,
	DES3D=		1,
	DES3EEE=	0,
	DES3EDE=	2,
	DES3DED=	5,
	DES3DDD=	7,
};

typedef struct DES3state DES3state;
struct DES3state
{
	unsigned long	setup;
	unsigned char	key[3][8];		/* unexpanded key */
	unsigned long	expanded[3][32];	/* expanded key */
	unsigned char	ivec[8];		/* initialization vector */
};

void	setupDES3state(DES3state *s, unsigned char key[3][8], unsigned char *ivec);
void	triple_block_cipher(unsigned long keys[3][32], unsigned char*, int);
void	des3CBCencrypt(unsigned char*, int, DES3state*);
void	des3CBCdecrypt(unsigned char*, int, DES3state*);
void	des3ECBencrypt(unsigned char*, int, DES3state*);
void	des3ECBdecrypt(unsigned char*, int, DES3state*);

/////////////////////////////////////////////////////////
// digests
/////////////////////////////////////////////////////////

enum
{
	SHA1dlen=	20,	/* SHA digest length */
	MD4dlen=	16,	/* MD4 digest length */
	MD5dlen=	16	/* MD5 digest length */
};

typedef struct DigestState DigestState;
struct DigestState
{
	unsigned long len;
	u32int state[5];
	unsigned char buf[128];
	int blen;
	char malloced;
	char seeded;
};
typedef struct DigestState SHAstate;
typedef struct DigestState MD5state;
typedef struct DigestState MD4state;

DigestState* md4(unsigned char*, unsigned long, unsigned char*, DigestState*);
DigestState* md5(unsigned char*, unsigned long, unsigned char*, DigestState*);
DigestState* sha1(unsigned char*, unsigned long, unsigned char*, DigestState*);
DigestState* hmac_md5(unsigned char*, unsigned long, unsigned char*, unsigned long, unsigned char*, DigestState*);
DigestState* hmac_sha1(unsigned char*, unsigned long, unsigned char*, unsigned long, unsigned char*, DigestState*);

/////////////////////////////////////////////////////////
// base 64 & 32 conversions
/////////////////////////////////////////////////////////

int	dec64(unsigned char *out, int lim, char *in, int n);
int	enc64(char *out, int lim, unsigned char *in, int n);
int	dec32(unsigned char *out, int lim, char *in, int n);
int	enc32(char *out, int lim, unsigned char *in, int n);

/////////////////////////////////////////////////////////
// random number generation
/////////////////////////////////////////////////////////
void	genrandom(unsigned char *buf, int nbytes);
void	prng(unsigned char *buf, int nbytes);

/////////////////////////////////////////////////////////
// primes
/////////////////////////////////////////////////////////
void	genprime(mpint *p, int n, int accuracy); // generate an n bit probable prime
void	gensafeprime(mpint *p, mpint *alpha, int n, int accuracy);	// prime and generator
void	genstrongprime(mpint *p, int n, int accuracy);	// generate an n bit strong prime
void	DSAprimes(mpint *q, mpint *p, unsigned char seed[SHA1dlen]);
int	probably_prime(mpint *n, int nrep);	// miller-rabin test
int	smallprimetest(mpint *p);		// returns -1 if not prime, 0 otherwise

/////////////////////////////////////////////////////////
// rc4
/////////////////////////////////////////////////////////
typedef struct RC4state RC4state;
struct RC4state
{
	 unsigned char state[256];
	 unsigned char x;
	 unsigned char y;
};

void	setupRC4state(RC4state*, unsigned char*, int);
void	rc4(RC4state*, unsigned char*, int);
void	rc4skip(RC4state*, int);
void	rc4back(RC4state*, int);

/////////////////////////////////////////////////////////
// rsa
/////////////////////////////////////////////////////////
typedef struct RSApub RSApub;
typedef struct RSApriv RSApriv;

// public/encryption key
struct RSApub
{
	mpint	*n;	// modulus
	mpint	*ek;	// exp (encryption key)
};

// private/decryption key
struct RSApriv
{
	RSApub	pub;

	mpint	*dk;	// exp (decryption key)

	// precomputed values to help with chinese remainder theorem calc
	mpint	*p;
	mpint	*q;
	mpint	*kp;	// dk mod p-1
	mpint	*kq;	// dk mod q-1
	mpint	*c2;	// for converting modular rep to answer
};

RSApriv*	rsagen(int nlen, int elen, int rounds);
mpint*		rsaencrypt(RSApub *k, mpint *in, mpint *out);
mpint*		rsadecrypt(RSApriv *k, mpint *in, mpint *out);
RSApub*		rsapuballoc(void);
void		rsapubfree(RSApub*);
RSApriv*	rsaprivalloc(void);
void		rsaprivfree(RSApriv*);
RSApub*		rsaprivtopub(RSApriv*);

/////////////////////////////////////////////////////////
// eg
/////////////////////////////////////////////////////////
typedef struct EGpub EGpub;
typedef struct EGpriv EGpriv;
typedef struct EGsig EGsig;

// public/encryption key
struct EGpub
{
	mpint	*p;	// modulus
	mpint	*alpha;	// generator
	mpint	*key;	// (encryption key) alpha**secret mod p
};

// private/decryption key
struct EGpriv
{
	EGpub	pub;
	mpint	*secret; // (decryption key)
};

// signature
struct EGsig
{
	mpint	*r, *s;
};

EGpriv*		eggen(int nlen, int rounds);
mpint*		egencrypt(EGpub *k, mpint *in, mpint *out);
mpint*		egdecrypt(EGpriv *k, mpint *in, mpint *out);
EGsig*		egsign(EGpriv *k, mpint *m);
int		egverify(EGpub *k, EGsig *sig, mpint *m);
EGpub*		egpuballoc(void);
void		egpubfree(EGpub*);
EGpriv*		egprivalloc(void);
void		egprivfree(EGpriv*);
EGsig*		egsigalloc(void);
void		egsigfree(EGsig*);
EGpub*		egprivtopub(EGpriv*);
