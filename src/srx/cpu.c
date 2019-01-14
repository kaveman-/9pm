void
main(int argc, char *argv[])
{
	int i;
	int so;

	argv0 = argv[0];

	for(i=1; i<argc; i++) {
		char *p = argv[i];
		if(p[0] != '-')
			break;
		if(strcmp(p, "-h") == 0) {
			i++;
			host = argv[i];
		} else
			usage();
	}

	if(i != argc)
		usage();

	if(host == NULL) {
		host = getenv("cpu");
		if(host == NULL)
			blFatal("no host specified: use -h host or set $cpu");
	}

	init();
	so = dial(host, Port);
	if(so < 0)
		blFatal("could not dial: %s", blGetError());
	
}
