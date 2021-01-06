#include <u.h>
#include <libc.h>
#include <thread.h>
#include <fcall.h>
#include <9p.h>
#include <json.h>

extern char running;

struct IRCServer {
	unsigned long cid;
	File *f;
	struct IRCServer *next;
};

struct IRCServer *ircservers;

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

static void
fsend(Srv *s)
{
	running = 0;
}

Srv fssrv = {
	.read = fsread,
	.write = fswrite,
	.end = fsend,
};

void
allocserver(JSON *json)
{
	JSON *jsonm;
	unsigned long cid;
	struct IRCServer *cur;

	jsonm = jsonbyname(json, "cid");
	if (jsonm == nil)
		sysfatal("allocbuffer: jsonbyname(cid): %r");
	cid = (unsigned long)jsonm->n;

	jsonm = jsonbyname(json, "name");
	if (jsonm == nil)
		sysfatal("allocserver: jsonbyname(name): %r");

	if (ircservers == nil) {
		ircservers = calloc(1, sizeof(struct IRCServer));
		cur = ircservers;
	} else {
		cur = ircservers;
		while(cur->next != nil) {
			if (cur->cid == cid)
				return; // server already alloced
			cur = cur->next;
		}

		cur->next = calloc(1, sizeof(struct IRCServer));
		cur = cur->next;
	}

	cur->cid = cid;
	cur->f = createfile(fssrv.tree->root, jsonm->s, nil, DMDIR|0777, nil);
}

void
allocbuffer(JSON *json)
{
	JSON *jsonm;
	unsigned long cid;
	struct IRCServer *cur;

	jsonm = jsonbyname(json, "cid");
	if (jsonm == nil)
		sysfatal("allocbuffer: jsonbyname(cid): %r");
	cid = (unsigned long)jsonm->n;

	jsonm = jsonbyname(json, "name");
	if (jsonm == nil)
		sysfatal("allocbuffer: jsonbyname: %r");

	cur = ircservers;
	while(cur != nil) {
		if (cur->cid == cid)
			break;
		cur = cur->next;
	}

	if (cur == nil)
		sysfatal("allocbuffer: server not found");

	createfile(cur->f, jsonm->s, nil, DMDIR|0777, nil);
}

void
startfs(void)
{
	char *srvname = smprint("irccloud.%d", getpid());

	ircservers = nil;

	fssrv.tree = alloctree(nil, nil, DMDIR|0777, nil);
	postmountsrv(&fssrv, srvname, "/n/irccloud", 0);

	free(srvname);
}
