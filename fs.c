#include <u.h>
#include <libc.h>
#include <thread.h>
#include <fcall.h>
#include <9p.h>
#include <json.h>
#include "dat.h"
#include "fns.h"

extern char running;

struct IRCServer *ircservers;
struct Buffer *buffers;

static void
fsread(Req *r)
{
	File *f = r->fid->file;
	struct Buffer *buffer;

	if (strcmp(f->name, "data") == 0) {
		buffer = f->aux;
		readbuf(r, buffer->data, buffer->length);
		respond(r, nil);
		return;
	}

	respond(r, "no");
}

static void
fswrite(Req *r)
{
	File *f = r->fid->file;
	struct Buffer *buffer = f->aux;

	if (strcmp(f->name, "data") == 0) {
		if (strcmp(buffer->name, "*") != 0) {
			say(buffer->server->cid, buffer->name, r->ifcall.data, r->ifcall.count);
			r->ofcall.count = r->ifcall.count;
			respond(r, nil);
			return;
		}
	}

	respond(r, "no");
}

static void
fsstat(Req *r)
{
	File *f = r->fid->file;

	if (strcmp(f->name, "data") == 0)
		r->d.length = ((struct Buffer*)f->aux)->length;

	respond(r, nil);
}

static void
fsend(Srv *s)
{
	running = 0;
}

Srv fssrv = {
	.read = fsread,
	.write = fswrite,
	.stat = fsstat,
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
	unsigned long timeout = 0;
	unsigned long deferred = 0;

	jsonm = jsonbyname(json, "timeout");
	if (jsonm != nil)
		timeout = (unsigned long)jsonm->n;

	jsonm = jsonbyname(json, "deferred");
	if (jsonm != nil)
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
	buffer->name = strdup(jsonm->s);
	buffer->server = server;
	buffer->f = createfile(server->f, jsonm->s, nil, DMDIR|0777, server);
	buffer->dataf = createfile(buffer->f, "data", nil, 0666, buffer);
	buffer->data = malloc(1);
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
