#include <u.h>
#include <libc.h>
#include <bio.h>
#include "fns.h"

void
main(int argc, char **argv)
{
	char *email;
	char *password;
	Biobuf *bin;

	bin = Bfdopen(0, OREAD);

	print("email: ");
	email = Brdstr(bin, '\n', 1);
	print("password: ");
	password = Brdstr(bin, '\n', 1);

	login(email, password);

	startfs();
	readstream();
}
