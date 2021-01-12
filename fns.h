int login(char*, char*);
void readstream(void);
void say(vlong, char*, char*, unsigned long);

void startfs(void);
void allocserver(JSON*);
void allocbuffer(JSON*);
struct Buffer* findbuffer(vlong);
struct IRCServer* findserver(vlong);
