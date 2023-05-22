#include <lib.h>

static char *path = "/lib/d", *path2 = "/lib/d";

int main() {
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		user_panic("failed to open %s, return value: %d", path, fd);
	}
	char buf[16];
	int len = read(fd, buf, sizeof(buf) - 1);
	buf[len] = '\0';
	debugf("File content: %s\n", buf);
	close(fd);
	fd = open(path2, O_RDONLY);
	if (fd < 0) {
		user_panic("failed to open %s, return value: %d", path2, fd);
	}
	len = read(fd, buf, sizeof(buf) - 1);
	buf[len] = '\0';
	debugf("File content: %s\n", buf);
	close(fd);
	return 0;
}
