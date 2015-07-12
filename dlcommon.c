#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "dlcommon.h"
#include "err_handler.h"


int retcode(char *s)
{
	char *p, *ep;
	int code;

	if ((p = strchr(s, ' ')) != NULL) {
		p += 1;
		code = strtol(p, &ep, 10);
		return code;
	}
	return -1;
}

void nwrite(int fd, const void *buf, unsigned int len)
{
	unsigned int n;

	while (len > 0) {
		n = write(fd, buf, len);
		if (n == -1) {
			if (errno == EPIPE)
				err_exit(errno, "nwrite");
			err_msg(errno, "nwrite");
			continue;
		}
			
		len -= n;
		buf += n;
	}
}
