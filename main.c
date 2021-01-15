#include <u.h>
#include <libc.h>
#include <json.h>
#include <thread.h>
#include <fcall.h>
#include <9p.h>
#include <auth.h>
#include "dat.h"
#include "fns.h"

void
threadmain(int argc, char **argv)
{
	UserPasswd *up;
	int r;

	up = auth_getuserpasswd(auth_getkey, "proto=pass service=irccloud");
	if (up == nil)
		return;

	irccloudlogin(up->user, up->passwd);
	memset(up->passwd, '\0', strlen(up->passwd));
	free(up);

	procrfork(readstream, nil, mainstacksize, RFNOTEG);
	startfs();
	threadexits(nil);
}
