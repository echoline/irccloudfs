#include <u.h>
#include <libc.h>
#include <json.h>
#include <bio.h>
#include <thread.h>
#include <fcall.h>
#include <9p.h>
#include "dat.h"
#include "fns.h"

char *session;
char *token;
unsigned long sinceid = -1;
char running;
char *streamid;
char backlog = 0;

extern struct Buffer *buffers;

#define URL "https://www.irccloud.com"
#define AUTHURL URL "/chat/auth-formtoken"
#define LOGINURL URL "/chat/login"
#define STREAMURL URL "/chat/stream"
#define SAYURL URL "/chat/say"

void readbacklog(char *path);

int
openurl(char *url, char **headers, int post, char *postdata, char *type)
{
	int clonefd;
	int bodyfd;
	int postfd;
	int r, i;
	char d[64];
	char *buf;

	clonefd = open("/mnt/web/clone", ORDWR);
	if (clonefd < 0)
		sysfatal("open /mnt/web/clone: %r");

	r = read(clonefd, d, sizeof(d) - 1);
	if (r < 0)
		sysfatal("read /mnt/web/clone: %r");
	d[r] = '\0';
	d[strcspn(d, "\r\n")] = '\0';

	r = strlen(url) + 4;
	buf = smprint("url %s", url);
	if (write(clonefd, buf, r) != r)
		sysfatal("write clonefd url: %r");
	free(buf);

	if (headers) {
		for (i = 0; headers[i] != nil; i++) {
			buf = smprint("headers %s", headers[i]);
			r = strlen(buf);
			if (write(clonefd, buf, r) != r)
				sysfatal("write clonefd headers: %r");
			free(buf);
		}
	}

	r = strlen("useragent irccloudfs");
	if (write(clonefd, "useragent irccloudfs", r) != r)
		sysfatal("write clonefd useragent: %r");

	if (post) {
		r = strlen("request POST");
		if (write(clonefd, "request POST", r) != r)
			sysfatal("write clonefd method: %r");

		if (type) {
			buf = smprint("contenttype %s", type);
			r = strlen(buf);
			if (write(clonefd, buf, r) != r)
				sysfatal("write clonefd contenttype: %r");
		}

		if (postdata) {
			buf = smprint("/mnt/web/%s/postbody", d);
			postfd = open(buf, OWRITE);
			r = strlen(postdata);
			if (write(postfd, postdata, r) != r)
				sysfatal("write postbody: %r");
			close(postfd);
			free(buf);
		}
	}

	buf = smprint("/mnt/web/%s/body", d);
	bodyfd = open(buf, OREAD);
	if (bodyfd < 0)
		sysfatal("open bodyfd: %r");
	free(buf);

	close(clonefd);

	return bodyfd;
}

char*
readbody(int bodyfd)
{
	char *buf;
	int i, r;

	buf = malloc(8192);
	buf[0] = '\0';
	i = 0;
	while ((r = read(bodyfd, buf + i, 8192-1)) > 0) {
		i += r;
		buf[i] = '\0';
		buf = realloc(buf, i + 8192);
	}

	if (r < 0)
		sysfatal("read bodyfd: %r");

	return buf;
}

int
login(char *username, char *password)
{
	JSON *json, *jsonm;
	char *buf;
	int fd;
	char **headers;
	char *postdata;

	fd = openurl(AUTHURL, nil, 1, "", nil);
	buf = readbody(fd);
	if (buf[0] == '\0')
		sysfatal("short authtoken read");
	json = jsonparse(buf);
	if (json == nil)
		sysfatal("jsonparse: %r");
	free(buf);

	jsonm = jsonbyname(json, "success");
	if (jsonm == nil)
		sysfatal("jsonbyname: %r");
	if (jsonm->n == 1) {
		jsonm = jsonbyname(json, "token");
		if (jsonm == nil)
			sysfatal("jsonbyname(token): %r");

		token = strdup(jsonm->s);
	} else
		sysfatal("authtoken read failure");

	jsonfree(json);
	close(fd);

	postdata = smprint("email=%s&password=%s&token=%s", username, password, token);
	headers = calloc(2, sizeof(char*));
	headers[0] = smprint("x-auth-formtoken: %s", token);

	fd = openurl(LOGINURL, headers, 1, postdata, nil);
	free(headers[0]);
	free(headers);
	free(postdata);
	buf = readbody(fd);
	if (buf[0] == '\0')
		sysfatal("short login read");
	json = jsonparse(buf);
	if (json == nil)
		sysfatal("jsonparse: %r");
	free(buf);

	jsonm = jsonbyname(json, "success");
	if (jsonm == nil)
		sysfatal("jsonbyname: %r");
	if (jsonm->n == 1) {
		jsonm = jsonbyname(json, "session");
		if (jsonm == nil)
			sysfatal("jsonbyname(session): %r");

		session = strdup(jsonm->s);
	} else
		sysfatal("login failure");

	jsonfree(json);
	close(fd);

	return 0;
}

void
parsestream(JSON *json)
{
	JSON *jsonm, *jsonm2, *jsonm3, *jsonm4, *jsonm5;
	JSONEl *next;
	unsigned long bid;
	struct Buffer *buffer;
	struct User *user, *members;
	char *msg;

	jsonm = jsonbyname(json, "eid");
	if (jsonm != nil && jsonm->n > 0)
		sinceid = (unsigned long)jsonm->n;

	jsonm = jsonbyname(json, "type");
	if (jsonm == nil)
		sysfatal("jsonbyname(type): %r");

	if (strcmp(jsonm->s, "header") == 0) {
		jsonm = jsonbyname(json, "streamid");
		if (streamid != nil)
			free(streamid);
		streamid = strdup(jsonm->s);
	}
	else if (strcmp(jsonm->s, "oob_include") == 0) {
		jsonm = jsonbyname(json, "url");
		backlog = 1;
		readbacklog(jsonm->s);
	}
	else if (strcmp(jsonm->s, "backlog_complete") == 0) {
		backlog = 0;
	}
	else if (strcmp(jsonm->s, "makeserver") == 0) {
		allocserver(json);
	}
	else if (strcmp(jsonm->s, "makebuffer") == 0) {
		jsonm = jsonbyname(json, "archived");
		if (jsonm == nil || jsonm->n == 0) {
			allocbuffer(json);
		}
	}
	else if (strcmp(jsonm->s, "buffer_msg") == 0
		|| strcmp(jsonm->s, "buffer_me_msg") == 0
		|| strcmp(jsonm->s, "notice") == 0) {
		jsonm3 = jsonbyname(json, "bid");
		if (jsonm3 == nil)
			sysfatal("jsonbyname(bid): %r");
		bid = (unsigned long)jsonm3->n;
		buffer = findbuffer(bid);
		if (buffer == nil)
			sysfatal("findbuffer: %r");
		jsonm3 = jsonbyname(json, "from");
		if (jsonm3 == nil) {
			jsonm3 = jsonbyname(json, "target");
			if (jsonm3 == nil)
				sysfatal("jsonbyname(target): %r");
		}
		jsonm2 = jsonbyname(json, "msg");
		if (jsonm2 == nil)
			sysfatal("jsonbyname(msg): %r");
		if (strcmp(jsonm->s, "buffer_msg") == 0)
			msg = smprint("%s → %s\n", jsonm3->s, jsonm2->s);
		else if (strcmp(jsonm->s, "notice") == 0)
			msg = smprint("NOTICE %s → %s\n", jsonm3->s, jsonm2->s);
		else
			msg = smprint("* %s %s\n", jsonm3->s, jsonm2->s);

		buffer->data = realloc(buffer->data, buffer->length + strlen(msg));
		memcpy(buffer->data + buffer->length, msg, strlen(msg));
		buffer->length += strlen(msg);
		free(msg);
	}
	else if (strcmp(jsonm->s, "channel_init") == 0) {
		jsonm = jsonbyname(json, "bid");
		if (jsonm == nil)
			sysfatal("jsonbyname(bid): %r");
		bid = (unsigned long)jsonm->n;
		buffer = findbuffer(bid);
		if (buffer == nil)
			sysfatal("findbuffer: %r");

		jsonm = jsonbyname(json, "mode");
		if (jsonm == nil)
			sysfatal("jsonbyname(mode): %r");
		if (buffer->mode != nil)
			free(buffer->mode);
		buffer->mode = strdup(jsonm->s);

		jsonm2 = jsonbyname(json, "topic");
		if (jsonm2 == nil)
			sysfatal("jsonbyname(topic): %r");
		jsonm3 = jsonbyname(jsonm2, "time");
		if (jsonm3 == nil)
			sysfatal("jsonbyname(time): %r");
		jsonm4 = jsonbyname(jsonm2, "nick");
		if (jsonm4 == nil)
			sysfatal("jsonbyname(nick): %r");
		jsonm5 = jsonbyname(jsonm2, "text");
		if (jsonm5 == nil)
			sysfatal("jsonbyname(text): %r");
		if (buffer->topic != nil)
			free(buffer->topic);
		buffer->topic = smprint("%s %c%s\ntopic set at %d by %s:\n%s\n", buffer->name, buffer->mode[0] == '\0'? ' ': '+', buffer->mode, (unsigned long)jsonm3->n, jsonm4->s, jsonm5->s);

		jsonm2 = jsonbyname(json, "members");
		if (jsonm2 == nil)
			sysfatal("jsonbyname(members): %r");

		members = buffer->members;
		for(next = jsonm2->first; next != nil; next = next->next) {
			jsonm2 = next->val;

			jsonm3 = jsonbyname(jsonm2, "nick");
			if (jsonm3 == nil)
				sysfatal("jsonbyname(nick): %r");

			jsonm5 = jsonbyname(jsonm2, "mode");
			if (jsonm5 == nil)
				sysfatal("jsonbyname(mode): %r");

			for(user = members; user != nil; user = user->next)
				if (strcmp(user->nick, jsonm3->s) == 0) {
					free(user->mode);
						user->mode = strdup(jsonm5->s);
					break;
				}
			if (user != nil)
				continue;

			user = calloc(1, sizeof(struct User));
			user->nick = strdup(jsonm3->s);
			user->mode = strdup(jsonm5->s);
			user->next = members;
			members = user;
		}
		buffer->members = members;
	} else if (strcmp(jsonm->s, "joined_channel") == 0) {
		jsonm = jsonbyname(json, "bid");
		if (jsonm == nil)
			sysfatal("jsonbyname(bid): %r");
		bid = (unsigned long)jsonm->n;
		buffer = findbuffer(bid);
		if (buffer == nil)
			sysfatal("findbuffer: %r");

		jsonm = jsonbyname(json, "nick");
		if (jsonm == nil)
			sysfatal("jsonbyname(nick): %r");

		jsonm2 = jsonbyname(json, "chan");
		if (jsonm2 == nil)
			sysfatal("jsonbyname(chan): %r");

		for (members = buffer->members; members != nil; members = members->next)
			if (strcmp(jsonm->s, members->nick) == 0)
				break;

		if (members == nil) {
			user = calloc(1, sizeof(struct User));
			user->nick = strdup(jsonm->s);
			user->mode = calloc(1, sizeof(char));
			user->next = buffer->members;
			buffer->members = user;
		}

		msg = smprint("JOIN %s to %s\n", jsonm->s, jsonm2->s);
		buffer->data = realloc(buffer->data, buffer->length + strlen(msg));
		memcpy(buffer->data + buffer->length, msg, strlen(msg));
		buffer->length += strlen(msg);
		free(msg);
	} else if (strcmp(jsonm->s, "parted_channel") == 0) {
		jsonm = jsonbyname(json, "bid");
		if (jsonm == nil)
			sysfatal("jsonbyname(bid): %r");
		bid = (unsigned long)jsonm->n;
		buffer = findbuffer(bid);
		if (buffer == nil)
			sysfatal("findbuffer: %r");

		jsonm = jsonbyname(json, "nick");
		if (jsonm == nil)
			sysfatal("jsonbyname(nick): %r");

		jsonm2 = jsonbyname(json, "chan");
		if (jsonm2 == nil)
			sysfatal("jsonbyname(chan): %r");

		if (buffer->members != nil) {
			if (strcmp(jsonm->s, buffer->members->nick) == 0) {
				free(buffer->members->nick);
				free(buffer->members->mode);
				members = buffer->members->next;
				free(buffer->members);
				buffer->members = members;
			} else for (members = buffer->members->next, user = buffer->members; members != nil; user = members, members = members->next) {
				if (strcmp(jsonm->s, members->nick) == 0) {
					free(members->nick);
					free(members->mode);
					user->next = members->next;
					free(members);
				}
			}
		}

		msg = smprint("PART %s from %s\n", jsonm->s, jsonm2->s);
		buffer->data = realloc(buffer->data, buffer->length + strlen(msg));
		memcpy(buffer->data + buffer->length, msg, strlen(msg));
		buffer->length += strlen(msg);
		free(msg);
	} else if (strcmp(jsonm->s, "quit") == 0) {
		jsonm = jsonbyname(json, "bid");
		if (jsonm == nil)
			sysfatal("jsonbyname(bid): %r");
		bid = (unsigned long)jsonm->n;
		buffer = findbuffer(bid);
		if (buffer == nil)
			sysfatal("findbuffer: %r");

		jsonm = jsonbyname(json, "nick");
		if (jsonm == nil)
			sysfatal("jsonbyname(nick): %r");

		jsonm2 = jsonbyname(json, "msg");
		if (jsonm2 == nil)
			sysfatal("jsonbyname(msg): %r");

		if (buffer->members != nil) {
			if (strcmp(jsonm->s, buffer->members->nick) == 0) {
				free(buffer->members->nick);
				free(buffer->members->mode);
				members = buffer->members->next;
				free(buffer->members);
				buffer->members = members;
			} else for (members = buffer->members->next, user = buffer->members; members != nil; user = members, members = members->next) {
				if (strcmp(jsonm->s, members->nick) == 0) {
					free(members->nick);
					free(members->mode);
					user->next = members->next;
					free(members);
				}
			}
		}

		msg = smprint("QUIT %s - %s\n", jsonm->s, jsonm2->s);
		buffer->data = realloc(buffer->data, buffer->length + strlen(msg));
		memcpy(buffer->data + buffer->length, msg, strlen(msg));
		buffer->length += strlen(msg);
		free(msg);
//	} else {
//		print("%s ", jsonm->s);
	}
}

void
readbacklog(char *path)
{
	char *url;
	int fd;
	Biobuf *stream;
	char *line;
	char *tmp = nil;
	char **headers;
	int l;
	JSON *json;
	JSONEl *next;
	struct Buffer *buffer;

	headers = calloc(2, sizeof(char*));
	headers[0] = smprint("Cookie: session=%s", session);

	url = smprint("%s%s\n", URL, path);
	fd = openurl(url, headers, 0, nil, nil);
	tmp = readbody(fd);
	json = jsonparse(tmp);
	if (json == nil)
		sysfatal("jsonparse: %r");
	free(tmp);
	for (next = json->first; next != nil; next = next->next)
		parsestream(next->val);
	jsonfree(json);

	free(headers[0]);
	free(headers);

	free(url);
}

void
readstream(void)
{
	JSON *json, *jsonm;
	char *buf;
	int fd;
	char **headers;
	char *tmp;
	int l;
	Biobuf *stream;
	streamid = nil;

	JSONfmtinstall();

	headers = calloc(2, sizeof(char*));
	headers[0] = smprint("Cookie: session=%s", session);

	fd = openurl(STREAMURL, headers, 0, nil, nil);
	stream = Bfdopen(fd, OREAD);
	running = 1;
	while(running) {
		buf = Brdline(stream, '\n');
		if (buf == nil) {
			close(fd);
			fd = openurl(STREAMURL, headers, 0, nil, nil);
			Binit(stream, fd, OREAD);
			continue;
		}
		l = Blinelen(stream);
		tmp = malloc(l + 1);
		memcpy(tmp, buf, l);
		tmp[l] = '\0';
		json = jsonparse(tmp);
		if (json == nil)
			sysfatal("jsonparse: %r");

		parsestream(json);

		jsonfree(json);
		free(tmp);
	}
}

void
say(unsigned long cid, char *to, char *data, unsigned long count)
{
	char *postdata;
	int fd;
	char **headers = calloc(3, sizeof(char*));
	char *msg = malloc(count+1);

	memcpy(msg, data, count);
	msg[count] = '\0';

	postdata = smprint("cid=%d&to=%s&msg=%s&token=%s&session=%s", cid, to, msg, token, session);

	headers[0] = smprint("x-auth-formtoken: %s", token);
	headers[1] = smprint("Cookie: session=%s", session);

	fd = openurl(SAYURL, headers, 1, postdata, nil);
	close(fd);

	free(msg);
	free(postdata);
	free(headers[0]);
	free(headers[1]);
	free(headers);
}
