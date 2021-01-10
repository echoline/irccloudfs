int login(char*, char*);
void readstream(void);
void say(unsigned long, char*, char*, unsigned long);

void startfs(void);
void allocserver(JSON*);
void allocbuffer(JSON*);
struct Buffer* findbuffer(unsigned long);
struct IRCServer* findserver(unsigned long);
