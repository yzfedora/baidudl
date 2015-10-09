#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err_handler.h>

#define BUFSZ	(1 << 20)
#define OUTLEN	(64)

static char *bstrstr(char *src, size_t src_len,
			char *dst, size_t dst_len)
{
	char *p1 = src;
	char *p2 = dst;
	char *saved_ptr;
	unsigned int matched = 0;

	while (p1 < src + src_len) {
		if (*p1++ == *p2++) {
			if (++matched == dst_len) {
				return (p1 - dst_len);
			}
			if (matched == 1)
				saved_ptr = p1;
		} else {
			if (matched > 0)
				p1 = saved_ptr;
			p2 = dst;
			matched = 0;
		}
	}

	return NULL;
}

static void bsearch(int fd, char *str)
{
	char buf[BUFSZ];
	char *match;
	char *ptr = buf;
	size_t ptr_len = 0;
	size_t str_len = strlen(str);
	ssize_t nread;
	size_t offset = 0;
	size_t total = lseek(fd, 0, SEEK_END);

	lseek(fd, 0, SEEK_SET);
	while ((nread = read(fd, buf, sizeof(buf)))) {
		printf("\rmatching ... %5.1f", (double)offset * 100 / total);
		
		if (nread > 0) {
			ptr = buf;
			ptr_len = nread;
match_next:
			if (!(match = bstrstr(ptr, ptr_len, str, str_len))) {
				offset += nread;
				continue;
			}
			
			/*
			 * Update the offset, and output found string.
			 */
			offset += (match - ptr);
			printf("\n%ld: %.*s\n", offset, OUTLEN, match);
			
			/* 
			 * Set pointer to next address, and new length of
			 * ptr_len.
			 */
			ptr = match + str_len;
			ptr_len -= (ptr - buf);

			goto match_next;
		} else {	/* error */
			if (errno == EINTR)
				continue;
			else
				err_sys("read");
		}
	}

}

int main(int argc, char *argv[])
{
	int fd;

	if (argc != 3) {
		err_msg("Usage: %s string file", argv[0]);
		return 0;
	}

	if ((fd = open(argv[2], O_RDONLY)) == -1)
		err_exit("failed to open %s", argv[2]);

	bsearch(fd, argv[1]);

	if (close(fd) == -1)
		err_sys("close");
	return 0;
}

