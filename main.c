#include <u.h>
#include <libc.h>
#include "fns.h"

void
usage(char *argv0)
{
	fprint(2, "usage:\n\t%s `{echo -n email | urlencode} `{echo -n password | urlencode}\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	if (argc < 3)
		usage(argv[0]);

	login(argv[1], argv[2]);

	startfs();
	readstream();
}
