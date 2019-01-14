#include <u.h>
#include <9pm/libc.h>
#include <9pm/srv.h>

/*
 *  make an address, add the defaults
 */
void
netmkaddr(char *addr, int naddr, char *linear, char *defnet, char *defsrv)
{
	char *cp;

	/*
	 *  dump network name
	 */
	cp = strchr(linear, '!');
	if(cp == 0){
		if(defnet==0){
			if(defsrv)
				snprint(addr, naddr, "net!%s!%s", linear, defsrv);
			else
				snprint(addr, naddr, "net!%s", linear);
		} else {
			if(defsrv)
				snprint(addr, naddr, "%s!%s!%s", defnet, linear, defsrv);
			else
				snprint(addr, naddr, "%s!%s", defnet, linear);
		}
		return;
	}

	/*
	 *  if there is already a service, use it
	 */
	cp = strchr(cp+1, '!');
	if(cp) {
		snprint(addr, naddr, linear);
		return;
	}

	/*
	 *  add default service
	 */
	if(defsrv == 0) {
		snprint(addr, naddr, linear);
		return;
	}

	snprint(addr, naddr, "%s!%s", linear, defsrv);
}
