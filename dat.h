struct IRCServer {
	unsigned long cid;
	File *f;
	struct IRCServer *next;
	char *nick;
};

struct User {
	char *nick;
	char *realname;
	char *mode;
	struct User *next;
};

struct Buffer {
	unsigned long bid;
	unsigned long cid;
	unsigned long timeout;
	unsigned long deferred;
	File *f;
	File *dataf;
	File *topicf;
	File *membersf;
	unsigned long last_eid;
	char *data;
	vlong length;
	struct Buffer *next;
	struct IRCServer *server;
	char *name;
	char *type;
	char *topic;
	char *mode;
	struct User *members;
};
