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
	struct Buffer *buffer = f->aux;
	struct User *user;

	if (f->parent != nil
		&& strcmp(f->parent->name, "members") == 0
		&& (buffer = f->parent->aux) != nil
		&& strcmp(buffer->type, "channel") == 0) {
		user = (struct User*)f->aux;
		if (user != nil) {
			readstr(r, user->mode);
			respond(r, nil);
			return;
		}
	}
	else if (strcmp(f->name, "data") == 0) {
		if (r->ifcall.offset < buffer->length) {
			readbuf(r, buffer->data, buffer->length);
			respond(r, nil);
		} else {
			if (nbsendp(buffer->reqchan, r) != 1)
				respond(r, "nbsendp failed");
		}
		return;
	}
	else if (strcmp(f->name, "topic") == 0
		&& strcmp(buffer->type, "channel") == 0) {
		readstr(r, buffer->topic);
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
		say(buffer->server->cid, buffer->name, r->ifcall.data, r->ifcall.count);
		r->ofcall.count = r->ifcall.count;
		respond(r, nil);
		return;
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
fsend(Srv*)
{
	running = 0;
}

static void
fsflush(Req *r)
{
	Req *check;
	struct Buffer *buffer;

	File *f = r->oldreq->fid->file;

	if (strcmp(f->name, "data") == 0) {
		if (r->oldreq->ifcall.type == Tread) {
			buffer = (struct Buffer*)f->aux;
			while((check = nbrecvp(buffer->reqchan)) != nil) {
				if (check == r->oldreq) {
					respond(check, "interrupted");
					break;
				}
				else
					sendp(buffer->reqchan, check);
			}
		}
	}

	respond(r, nil);
}

Srv fssrv = {
	.read = fsread,
	.write = fswrite,
	.stat = fsstat,
	.end = fsend,
	.flush = fsflush,
};

void
allocserver(JSON *json)
{
	JSON *jsonm;
	vlong cid;
	struct IRCServer *cur;
	char *status, *name, *nick;

	jsonm = jsonbyname(json, "cid");
	if (jsonm == nil)
		sysfatal("allocserver: jsonbyname(cid): %r");
	cid = (vlong)jsonm->n;

	jsonm = jsonbyname(json, "status");
	if (jsonm == nil)
		sysfatal("allocserver: jsonbyname(status): %r");
	status = jsonstr(jsonm);
	if (status == nil)
		return;
	if (strcmp(status, "disconnected") == 0)
		return;

	jsonm = jsonbyname(json, "name");
	if (jsonm == nil)
		sysfatal("allocserver: jsonbyname(name): %r");
	name = jsonstr(jsonm);
	if (name == nil)
		return;

	jsonm = jsonbyname(json, "nick");
	if (jsonm == nil)
		sysfatal("allocserver: jsonbyname(nick): %r");
	nick = jsonstr(jsonm);
	if (nick == nil)
		return;

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
	cur->nick = strdup(nick);
	cur->f = createfile(fssrv.tree->root, name, nil, DMDIR|0777, cur);
}

struct IRCServer*
findserver(vlong cid)
{
	struct IRCServer *server;

	server = ircservers;
	while (server != nil) {
		if (server->cid == cid)
			return server;
		server = server->next;
	}

	return nil;
}

struct Buffer*
findbuffer(vlong bid)
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
	vlong cid;
	vlong bid;
	struct IRCServer *server;
	struct Buffer *buffer;
	struct Buffer *buffer2;
	int timeout = 0;
	int deferred = 0;
	char *type;
	char *name;

	jsonm = jsonbyname(json, "timeout");
	if (jsonm != nil)
		timeout = jsonm->n;

	jsonm = jsonbyname(json, "deferred");
	if (jsonm != nil)
		deferred = jsonm->n;

	if (deferred != 0)
		return; // ???

	jsonm = jsonbyname(json, "cid");
	if (jsonm == nil)
		sysfatal("allocbuffer: jsonbyname(cid): %r");
	cid = (vlong)jsonm->n;

	jsonm = jsonbyname(json, "bid");
	if (jsonm == nil)
		sysfatal("allocbuffer: jsonbyname(bid): %r");
	bid = (vlong)jsonm->n;

	jsonm = jsonbyname(json, "buffer_type");
	if (jsonm == nil)
		sysfatal("allocbuffer: jsonbyname(buffer_type): %r");
	type = jsonstr(jsonm);

	jsonm = jsonbyname(json, "name");
	if (jsonm == nil)
		sysfatal("allocbuffer: jsonbyname(name): %r");
	name = jsonstr(jsonm);

	server = findserver(cid);

	if (server == nil || type == nil || name == nil)
		return;

	if (buffers == nil) {
		buffers = calloc(1, sizeof(struct Buffer));
		buffer = buffers;
	} else {
		buffer = buffers;
		buffer2 = buffer;
		while (buffer != nil) {
			if (buffer->bid == bid)
				return;
			if (strcmp(buffer->name, name) == 0)
				return;
			buffer2 = buffer;
			buffer = buffer->next;
		}
		buffer = buffer2;

		buffer->next = calloc(1, sizeof(struct Buffer));
		buffer = buffer->next;
	}

	buffer->timeout = timeout;
	buffer->deferred = deferred;
	buffer->cid = cid;
	buffer->bid = bid;
	buffer->type = strdup(type);
	buffer->name = strdup(name);
	buffer->server = server;
	buffer->f = createfile(server->f, name, nil, DMDIR|0777, server);
	buffer->dataf = createfile(buffer->f, "data", nil, 0666, buffer);
	buffer->data = smprint("%s:%lld - %s:%lld\n", server->f->name, cid, name, bid);
	buffer->length = strlen(buffer->data);
	buffer->reqchan = chancreate(sizeof(Req*), 16);
	if (strcmp(type, "channel") == 0) {
		buffer->topicf = createfile(buffer->f, "topic", nil, 0444, buffer);
		buffer->membersf = createfile(buffer->f, "members", nil, DMDIR|0555, buffer);
	}
}

void
startfs(void)
{
	char *srvname = smprint("irccloud.%d", getpid());

	ircservers = nil;
	buffers = calloc(1, sizeof(struct Buffer));

	fssrv.tree = alloctree(nil, nil, DMDIR|0777, nil);
	createfile(fssrv.tree->root, "data", nil, 0444, buffers);
	buffers->name = calloc(1, 1);
	buffers->data = calloc(1, 1);
	buffers->reqchan = chancreate(sizeof(Req*), 16);
	threadpostmountsrv(&fssrv, srvname, "/n/irccloud", 0);

	free(srvname);
}
