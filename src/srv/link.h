typedef struct Glink	Glink;
typedef struct Gmem	Gmem;

enum {
	Gbufsize	= 100,
};

struct Glink {
	HANDLE	eread;		/* client handle for read event object */
	HANDLE	ewrite;		/* client handle for write event object */
	int	closed;		/* read side closed */
	int	eof;		/* write side closed */
	int	reading;	/* read in progress */
	int	writing;	/* writein progress */
	int	ri;		/* only changed by reader */
	int	wi;		/* only changed by writer */
	uchar	buf[Gbufsize];	/* data */
};


struct Gmem {
	Glink	u;	/* to server */
	Glink	d;	/* to client */
};
