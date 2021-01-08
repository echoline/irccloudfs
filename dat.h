struct IRCServer {
	unsigned long cid;
	File *f;
	struct IRCServer *next;
};

struct Buffer {
	unsigned long bid;
	unsigned long cid;
	unsigned long timeout;
	unsigned long deferred;
	File *f;
	File *dataf;
	unsigned long last_eid;
	char *data;
	vlong length;
	struct Buffer *next;
	struct IRCServer *server;
	char *name;
};
