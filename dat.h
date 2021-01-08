struct Buffer {
	unsigned long bid;
	unsigned long cid;
	unsigned long timeout;
	unsigned long deferred;
	File *f;
	File *dataf;
	unsigned long last_eid;
	char *data;
	struct Buffer *next;
};
