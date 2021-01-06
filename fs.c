#include <u.h>
#include <libc.h>
#include <thread.h>
#include <fcall.h>
#include <9p.h>

static void
fsread(Req *r)
{
	respond(r, "no");
}

static void
fswrite(Req *r)
{
	respond(r, "no");
}

Srv fssrv = {
	.read = fsread,
	.write = fswrite,
};

void
startfs(void)
{
	char *srvname = smprint("irccloud.%d", getpid());

	fssrv.tree = alloctree(nil, nil, DMDIR|0777, nil);
	postmountsrv(&fssrv, srvname, "/n/irccloud", 0);

	free(srvname);
}
