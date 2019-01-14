#include <u.h>
#include <9pm/libc.h>

int
pipe(int fd[2])
{
	if(bind("#|", "/mnt", MREPL) < 0)
		return -1;
	fd[0] = open("/mnt/data", ORDWR);
	fd[1] = open("/mnt/data1", ORDWR);
print("fd[0] = %d fd[1] = %d\n", fd[0], fd[1]);
/*	unmount(0, "/mnt"); */
	return 0;	
}
