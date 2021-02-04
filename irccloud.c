#include <u.h>
#include <libc.h>

void
main(int, char**)
{
	char buf[8192];
	char path[1024];
	char *p;
	long n;
	int fd;
	int pid;

	getwd(path, 1024);

	p = strrchr(path, '/');
	if (p == nil)
		sysfatal("path");
	else
		p++;

	if (*p == '#') {
		chdir("members");
		sprint(path, "../data");
	} else {
		sprint(path, "data");
	}

	fd = open(path, ORDWR);
	if (fd < 0)
		sysfatal("open: %r");

	switch((pid = fork())) {
	case -1:
		sysfatal("fork: %r");
	case 0:
		while((n=read(fd, buf, (long)sizeof buf))>0)
			if(write(1, buf, n)!=n)
				sysfatal("write: %r");
		if(n < 0)
			sysfatal("read: %r");
	default:
		while((n=read(0, buf, (long)sizeof buf))>0)
			if(write(fd, buf, n)!=n)
				sysfatal("write: %r");
		if(n < 0)
			sysfatal("read: %r");
	}

	close(fd);

	if (pid > 0) {
		sprint(path, "/proc/%d/note", pid);
		fd = open(path, OWRITE);
		if (fd < 0)
			sysfatal("open: %r");
		write(fd, "hangup", 6);
	}
}
