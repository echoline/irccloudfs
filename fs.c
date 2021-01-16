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
	char *buf = nil;
	ulong blen = 0;
	char *tmp;
	ulong len;
	struct User *members;
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
fsend(Srv *s)
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
	JSON *jsonm, *nick;
	vlong cid;
	struct IRCServer *cur;

	jsonm = jsonbyname(json, "cid");
	if (jsonm == nil)
		sysfatal("allocserver: jsonbyname(cid): %r");
	cid = (vlong)jsonm->n;

	jsonm = jsonbyname(json, "status");
	if (jsonm == nil)
		sysfatal("allocserver: jsonbyname(status): %r");
	if (strcmp(jsonm->s, "disconnected") == 0)
		return;

	jsonm = jsonbyname(json, "name");
	if (jsonm == nil)
		sysfatal("allocserver: jsonbyname(name): %r");

	nick = jsonbyname(json, "nick");
	if (nick == nil)
		sysfatal("allocserver: jsonbyname(nick): %r");

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
	cur->nick = strdup(nick->s);
	cur->f = createfile(fssrv.tree->root, jsonm->s, nil, DMDIR|0777, cur);
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
	char timeout = 0;
	char deferred = 0;
	char *type;

	jsonm = jsonbyname(json, "timeout");
	if (jsonm != nil)
		timeout = (char)jsonm->n;

	jsonm = jsonbyname(json, "deferred");
	if (jsonm != nil)
		deferred = (char)jsonm->n;

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
	type = jsonm->s;

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
		return;

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
	buffer->type = strdup(type);
	buffer->name = strdup(jsonm->s);
	buffer->server = server;
	buffer->f = createfile(server->f, jsonm->s, nil, DMDIR|0777, server);
	buffer->dataf = createfile(buffer->f, "data", nil, 0666, buffer);
	buffer->data = malloc(1);
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
	buffers->data = malloc(1);
	buffers->reqchan = chancreate(sizeof(Req*), 16);
	threadpostmountsrv(&fssrv, srvname, "/n/irccloud", 0);

	free(srvname);
}
