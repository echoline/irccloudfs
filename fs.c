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
	unsigned long blen = 0;
	char *tmp;
	unsigned long len;
	struct User *members;

	if (strcmp(f->name, "data") == 0) {
		readbuf(r, buffer->data, buffer->length);
		respond(r, nil);
		return;
	}
	else if (strcmp(f->name, "topic") == 0
		&& strcmp(buffer->type, "channel") == 0) {
		readstr(r, buffer->topic);
		respond(r, nil);
		return;
	}
	else if (strcmp(f->name, "members") == 0
		&& strcmp(buffer->type, "channel") == 0) {
		for (members = buffer->members; members != nil; members = members->next) {
			tmp = smprint("%s %c%s\n", members->nick, members->mode[0] != '\0'? '+': ' ', members->mode);
			len = strlen(tmp);
			buf = realloc(buf, blen + len);
			memcpy(buf + blen, tmp, len);
			blen += len;
			free(tmp);
		}
		if (buf != nil) {
			readbuf(r, buf, blen);
			respond(r, nil);
			free(buf);
			return;
		}
	}

	respond(r, "no");
}

static void
fswrite(Req *r)
{
	File *f = r->fid->file;
	struct Buffer *buffer = f->aux;

	if (strcmp(f->name, "data") == 0) {
		if (r->ifcall.data[0] != '\n')
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
		sysfatal("allocserver: jsonbyname(cid): %r");
	cid = (unsigned long)jsonm->n;

	jsonm = jsonbyname(json, "status");
	if (jsonm == nil)
		sysfatal("allocserver: jsonbyname(status): %r");
	if (strcmp(jsonm->s, "disconnected") == 0)
		return;

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
	char *type;

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
	if (strcmp(type, "channel") == 0) {
		buffer->topicf = createfile(buffer->f, "topic", nil, 0444, buffer);
		buffer->membersf = createfile(buffer->f, "members", nil, 0444, buffer);
	}
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
