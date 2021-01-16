struct IRCServer {
	vlong cid;
	File *f;
	struct IRCServer *next;
	char *nick;
};

struct User {
	char *nick;
	char *realname;
	char *mode;
	File *f;
	struct User *next;
	struct Buffer *buffer;
};

struct Buffer {
	vlong bid;
	vlong cid;
	char timeout;
	char deferred;
	File *f;
	File *dataf;
	File *topicf;
	File *membersf;
	vlong last_eid;
	char *data;
	vlong length;
	struct Buffer *next;
	struct IRCServer *server;
	char *name;
	char *type;
	char *topic;
	char *mode;
	struct User *members;
	Channel *reqchan;
};
