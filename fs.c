#include <u.h>
#include <libc.h>
#include <thread.h>
#include <fcall.h>
#include <9p.h>
#include <json.h>
#include "dat.h"

extern char running;

struct IRCServer {
	unsigned long cid;
	File *f;
	struct IRCServer *next;
};
struct IRCServer *ircservers;

struct Buffer *buffers;

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

struct Buffer*
findbuffer(unsigned long bid)
{
	struct Buffer *buffer;

	buffer = buffers;
	while (buffer != nil) {
		if (buffer->bid == bid)
			return buffer;
		buffer = buffer->next;
	}

	return nil;
}

void
allocbuffer(JSON *json)
{
	JSON *jsonm;
	unsigned long cid;
	unsigned long bid;
	struct IRCServer *server;
	struct Buffer *buffer;
	unsigned long timeout;
	unsigned long deferred;

	jsonm = jsonbyname(json, "timeout");
	if (jsonm == nil)
		sysfatal("allocbuffer: jsonbyname(timeout): %r");
	timeout = (unsigned long)jsonm->n;

	jsonm = jsonbyname(json, "deferred");
	if (jsonm == nil)
		sysfatal("allocbuffer: jsonbyname(deferred): %r");
	deferred = (unsigned long)jsonm->n;

	jsonm = jsonbyname(json, "cid");
	if (jsonm == nil)
		sysfatal("allocbuffer: jsonbyname(cid): %r");
	cid = (unsigned long)jsonm->n;

	jsonm = jsonbyname(json, "bid");
	if (jsonm == nil)
		sysfatal("allocbuffer: jsonbyname(bid): %r");
	bid = (unsigned long)jsonm->n;

	jsonm = jsonbyname(json, "name");
	if (jsonm == nil)
		sysfatal("allocbuffer: jsonbyname(name): %r");

	server = ircservers;
	while(server != nil) {
		if (server->cid == cid)
			break;
		server = server->next;
	}

	if (server == nil)
		sysfatal("allocbuffer: server not found");

	if (buffers == nil) {
		buffers = calloc(1, sizeof(struct Buffer));
		buffer = buffers;
	} else {
		buffer = buffers;
		while (buffer->next != nil) {
			if (buffer->bid == bid)
				return;
			buffer = buffer->next;
		}

		buffer->next = calloc(1, sizeof(struct Buffer));
		buffer = buffer->next;
	}

	buffer->timeout = timeout;
	buffer->deferred = deferred;
	buffer->cid = cid;
	buffer->bid = bid;
	buffer->f = createfile(server->f, jsonm->s, nil, DMDIR|0777, nil);
	buffer->dataf = createfile(buffer->f, "data", nil, 0666, buffer);
}

void
startfs(void)
{
	char *srvname = smprint("irccloud.%d", getpid());

	ircservers = nil;
	buffers = nil;

	fssrv.tree = alloctree(nil, nil, DMDIR|0777, nil);
	postmountsrv(&fssrv, srvname, "/n/irccloud", 0);

	free(srvname);
}
