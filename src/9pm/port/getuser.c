#include <u.h>
#include <9pm/libc.h>

static Lock	lk;	
static char	user[NAMELEN];

char *
getuser(void)
{
	char *p;

	lock(&lk);
	if(user[0] == 0) {
		p = getenv("user");
		if(p == 0)
			strcpy(user, "none");
		else {
			strncpy(user, p, NAMELEN);
			user[NAMELEN-1] = 0;
			free(p);
		}
	}
	unlock(&lk);	
	return user;
}
