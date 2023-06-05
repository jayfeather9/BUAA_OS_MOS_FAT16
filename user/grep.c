#include <lib.h>

int fd;
char buf[8192], lbuf[8192];

void grep(char *pattern) {
	long n; int r;
	char *plb;
	plb = lbuf;
	int slen = strlen(pattern);
	// printf("fd = %d\n", fd);
	while ((n = read(fd, buf, (long)sizeof buf)) > 0) {
		// printf("read %d chars\n", n);
		for (int i = 0; i < n; i++) {
			if (buf[i] == '\n'){
				*plb++ = '\n';
				*plb = '\0';
				// printf("read a line: %s", lbuf);
				char *pp = pattern, *tpl = lbuf;
				int len = 0;
				for (; tpl < plb; tpl++) {
					// printf("%c ", *tpl);
					if (*pp == *tpl) {
						pp++;
						len++;
						if (len == slen) {
							printf(lbuf);
							break;
						}
					}
					else {
						pp = pattern;
						tpl = tpl - len;
						len = 0;
					}
				}
				plb = lbuf;
			}
			else {
				*plb++ = buf[i];
			}
		}
	}
	if (n < 0) {
		user_panic("error reading file: %d", n);
	}
}

int main(int argc, char **argv) {
	int f, i;
	// printf("argc = %d\n", argc);
	if(argc == 2) {
		fd = 0;		
	} else {
		fd = open(argv[2], O_RDONLY);
	}
	grep(argv[1]);
	return 0;
}
