#include <u.h>
#include <9pm/libc.h>

void
main(int argc, char *argv[])
{
	int i;
	Dir dir;
	char *user, *errs;

	ARGBEGIN{
	}ARGEND
	if(argc < 1){
		fprint(2, "usage: chown owner file ....\n");
		exits("usage");
	}
	user = argv[0];
	errs = 0;
	for(i=1; i<argc; i++){
		if(dirstat(argv[i], &dir) == -1){
			fprint(2, "chown: can't stat %s: %r\n", argv[i]);
			errs = "can't stat";
			continue;
		}
		strncpy(dir.uid, user, NAMELEN);
		if(dirwstat(argv[i], &dir) == -1){
			fprint(2, "chown: can't change owner for %s: %r\n", argv[i]);
			errs = "can't wstat";
		}
	}
	exits(errs);
}
