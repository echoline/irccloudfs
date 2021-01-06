#include <u.h>
#include <libc.h>
#include <json.h>
#include <bio.h>
#include "fns.h"

char *session;
char *token;
unsigned long sinceid = -1;
char running;

#define URL "https://www.irccloud.com"
#define AUTHURL URL "/chat/auth-formtoken"
#define LOGINURL URL "/chat/login"
#define STREAMURL URL "/chat/stream"

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

	buf = malloc(256);
	buf[0] = '\0';
	i = 0;
	while ((r = read(bodyfd, buf + i, 255)) > 0) {
		i += r;
		buf[i] = '\0';
		buf = realloc(buf, i + 256);
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
	JSON *jsonm;

	jsonm = jsonbyname(json, "eid");
	if (jsonm != nil && jsonm->n != -1)
		sinceid = (unsigned long)jsonm->n;

	jsonm = jsonbyname(json, "type");
	if (jsonm == nil)
		sysfatal("jsonbyname(type): %r");

	if (strcmp(jsonm->s, "makeserver") == 0) {
		allocserver(json);
	}
	else if (strcmp(jsonm->s, "makebuffer") == 0) {
		jsonm = jsonbyname(json, "archived");
		if (jsonm == nil)
			sysfatal("jsonbyname(archived): %r");
		if (jsonm->n == 0) {
			allocbuffer(json);
		}
	}
}

void
readbacklog(char *path, char *streamid)
{
	char *url;
	int fd;
	Biobuf *stream;
	char *line;
	char *tmp = nil;
	char **headers;
	char *postdata;
	int l, o;
	JSON *json;

	headers = calloc(2, sizeof(char*));
	headers[0] = smprint("Cookie: session=%s", session);
	postdata = smprint("streamid=%s", streamid);

	url = smprint("%s%s\n", URL, path);
	fd = openurl(url, headers, 0, postdata, nil);
	stream = Bfdopen(fd, OREAD);
	o = 0;
	while((line = Brdline(stream, '\n')) != nil) {
		l = Blinelen(stream);
		tmp = malloc(l + 1);
		memcpy(tmp, line, l);
		tmp[l] = '\0';
		json = jsonparse(tmp);
		if (json == nil)
			continue;
		free(tmp);
		parsestream(json);
		jsonfree(json);
	}

	free(headers[0]);
	free(headers);
	free(postdata);

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
	char *postdata;
	int l;
	Biobuf *stream;
	char *streamid = nil;

	headers = calloc(2, sizeof(char*));
	headers[0] = smprint("Cookie: session=%s", session);

	fd = openurl(STREAMURL, headers, 0, nil, nil);
	stream = Bfdopen(fd, OREAD);
	running = 1;
	while(running) {
		buf = Brdline(stream, '\n');
		if (buf == nil) {
			close(fd);
			postdata = smprint("since_id=%s&stream_id=%s", sinceid, streamid);
			fd = openurl(STREAMURL, headers, 1, postdata, nil);
			free(postdata);
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

		jsonm = jsonbyname(json, "type");
		if (strcmp(jsonm->s, "header") == 0) {
			jsonm = jsonbyname(json, "streamid");
			if (streamid != nil)
				free(streamid);
			streamid = strdup(jsonm->s);
		}
		else if (strcmp(jsonm->s, "oob_include") == 0) {
			jsonm = jsonbyname(json, "url");
			readbacklog(jsonm->s, streamid);
		}
		else
			parsestream(json);

		jsonfree(json);
		free(tmp);
	}
}
