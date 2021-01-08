#include <u.h>
#include <libc.h>
#include <bio.h>
#include <json.h>
#include <thread.h>
#include <fcall.h>
#include <9p.h>
#include "dat.h"
#include "fns.h"

void
main(int argc, char **argv)
{
	char *email;
	char *password;
	Biobuf *bin;
	int r;

	bin = Bfdopen(0, OREAD);

	print("email: ");
	email = Brdstr(bin, '\n', 1);
	print("password: ");
	password = Brdstr(bin, '\n', 1);

	login(email, password);
	memset(password, '\0', strlen(password));
	free(password);
	free(email);

	startfs();
	r = rfork(RFFDG|RFREND|RFPROC|RFMEM);
	if (r < 0)
		sysfatal("rfork: %r");
	if (r == 0)
		readstream();
	exits(nil);
}
