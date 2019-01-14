#include "sam.h"
#include "parse.h"

extern	jmp_buf	mainloop;

char	errfile[64];
String	plan9cmd;	/* null terminated */
Buffer	plan9buf;
void	checkerrs(void);

int
plan9(File *f, int type, String *s, int nest)
{
	uint pid;
	int stdin, stdout, stderr;
	int retcode;
	char *argv[6];
	int pipe1[2];

	if(s->s[0]==0 && plan9cmd.s[0]==0)
		error(Enocmd);
	else if(s->s[0])
		Strduplstr(&plan9cmd, s);
	if(downloaded)
		samerr(errfile);
	else
		strcpy(errfile, "/dev/tty");
	if(type!='!' && pipe(pipe1)==-1)
		error(Epipe);
	if(type=='|')
		snarf(f, addr.r.p1, addr.r.p2, &plan9buf, 1);
	if(downloaded)
		remove(errfile);

	stdin = -1;
	stdout = -1;
	stderr = create(errfile, OWRITE, 0666);
	if(type == '>')
		stdout = stderr;
	else if(type == '!'){
		stdout = stderr;
		stdin = -1;
	}
	if(type != '!') {
		if(type=='<' || type=='|')
			stdout = pipe1[1];
		else if(type == '>')
			stdin = pipe1[0];
	}

	/*
	 * fake up by dumping data to a temp file
	 */
	if(type == '|'){
		stdin = newtmp(1000);
		io = dup(stdin, -1);
		if(io < 0){
			close(stdin);
			stdin = -1;
		}else{
			bpipeok = 1;
			writeio(f);
			bpipeok = 0;
			closeio((Posn)-1);
			seek(stdin, 0, 0);
		}
	}

	argv[0] = "rcsh";
	argv[1] = "-I";
	argv[2] = "-c";
	argv[3] = Strtoc(&plan9cmd);
	argv[4] = 0;

	pid = proc(argv, stdin, stdout, stderr, 0);
	close(stdin);
	close(stdout);
	close(stderr);

	/*
	 * parent
	 */
	if(pid == 0)
		error(Efork);
	if(type=='<' || type=='|'){
		int nulls;
		if(downloaded && addr.r.p1 != addr.r.p2)
			outTl(Hsnarflen, addr.r.p2-addr.r.p1);
		snarf(f, addr.r.p1, addr.r.p2, &snarfbuf, 0);
		logdelete(f, addr.r.p1, addr.r.p2);
		close(pipe1[1]);
		io = pipe1[0];
		f->tdot.p1 = -1;
		f->ndot.r.p2 = addr.r.p2+readio(f, &nulls, 0, FALSE);
		f->ndot.r.p1 = addr.r.p2;
		closeio((Posn)-1);
	}else if(type=='>'){
		io = pipe1[1];
		bpipeok = 1;
		writeio(f);
		bpipeok = 0;
		closeio((Posn)-1);
	}
	retcode = procwait(pid, 0);
	if(type=='|' || type=='<')
		if(retcode!=0)
			warn(Wbadstatus);
	if(downloaded)
		checkerrs();
	if(!nest)
		dprint("!\n");
	return retcode;
}

void
checkerrs(void)
{
	char buf[256];
	int f, n, nl;
	char *p;
	long l;

	if(statfile(errfile, 0, 0, 0, &l, 0) > 0 && l != 0){
		if((f=open(errfile, OREAD)) != -1){
			if((n=read(f, buf, sizeof buf-1)) > 0){
				for(nl=0,p=buf; nl<3 && p<&buf[n]; p++)
					if(*p=='\n')
						nl++;
				*p = 0;
				dprint("%s", buf);
				if(p-buf < l-1)
					dprint("(sam: more in %s)\n", errfile);
			}
			close(f);
		}
	}else
		remove(errfile);
}
