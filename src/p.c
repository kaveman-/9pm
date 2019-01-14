#include <u.h>
#include <9pm/libc.h>
#include <9pm/bio.h>

#define	DEF	22	/* lines in chunk: 3*DEF == 66, #lines per nroff page */

Biobuf *cons;
Biobuf bout;

int pglen = DEF;

void printfile(int);

void
main(int argc, char *argv[])
{
	int n;
	int f;

	if((cons = Bopen("/dev/cons", OREAD)) == 0) {
		fprint(2, "p: can't open /dev/cons\n");
		exits("missing /dev/cons");
	}
	Binit(&bout, 1, OWRITE);
	n = 0;
	while(argc > 1) {
		--argc; argv++;
		if(*argv[0] == '-'){
			pglen = atoi(&argv[0][1]);
			if(pglen <= 0)
				pglen = DEF;
		} else {
			n++;
			f = open(argv[0], OREAD);
			if(f < 0){
				fprint(2, "p: can't open %s\n", argv[0]);
				continue;
			}
			printfile(f);
		}
	}
	if(n == 0)
		printfile(0);
	exits(0);
}

void
printfile(int f)
{
	int i, j, n;
	char *s, *cmd;
	Biobuf *b;
	char *argv[5];
	uint pid;

	b = malloc(sizeof(Biobuf));
	Binit(b, f, OREAD);
	for(;;){
		for(i=1; i <= pglen; i++) {
			s = Brdline(b, '\n');
			if(s == 0){
				n = BLINELEN(b);
				if(n > 0)	/* line too long for Brdline */
					for(j=0; j<n; j++)
						Bputc(&bout, Bgetc(b));
				else		/* true EOF */
					return;
			}else{
				Bwrite(&bout, s, Blinelen(b)-1);
				if(i < pglen)
					Bwrite(&bout, "\n", 1);
			}
		}
		Bflush(&bout);
	    getcmd:
		cmd = Brdline(cons, '\n');
		if(cmd == 0 || *cmd == 'q')
			exits(0);
		cmd[Blinelen(cons)-1] = 0;
		if(*cmd == '!'){
			argv[0] = "rcsh";
			argv[1] = "-c";
			argv[2] = cmd+1;
			argv[3] = 0;
			pid = proc(argv, Bfildes(cons), 1, 2, 0);
			if(pid == 0)
				fprint(2, "%s: proc failed: %r", argv0);
			else
				procwait(pid, 0);
			goto getcmd;
		}
	}
}
