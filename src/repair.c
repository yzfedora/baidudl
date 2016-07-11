#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "err_handler.h"
#include "dlcommon.h"

struct repair_range {
	size_t rr_start;
	size_t rr_end;
};

static off_t repair_seek(int fd, off_t offset, int whence)
{
	off_t rc = lseek(fd, offset, whence);
	if (rc == (off_t)-1)
		err_exit("repair_seek");
	return rc;
}

static off_t repair_get_offset(int fd)
{
	return repair_seek(fd, 0, SEEK_END);
}

static void usage(const char *progname)
{
	err_msg("Usage: %s file start-end ...", progname);
	exit(EXIT_FAILURE);
}

static int repair_open(const char *file)
{
	int fd;

	if ((fd = open(file, O_RDWR)) == -1)
		err_exit("repair_open");
	return fd;
}

static void repair_close(int fd)
{
	if (close(fd) == -1)
		err_sys("repair_close");
}

static void repair_write_nthreads(int fd, int nthreads)
{
	repair_seek(fd, 0, SEEK_END);
	printf("Write number of threads(%d) to offest %ld ...\n",
				nthreads, repair_get_offset(fd));
	writen(fd, &nthreads, sizeof(nthreads));
}

static void repair_write_range(int fd, size_t start, size_t end)
{
	repair_seek(fd, 0, SEEK_END);
	printf("Write range %ld-%ld to offest %ld ...\n", start, end,
				repair_get_offset(fd));
	writen(fd, &start, sizeof(start));
	writen(fd, &end, sizeof(end));
}

static void repair_doit(int fd, struct repair_range *range, int range_cnt)
{
	int i;

	if (range_cnt <= 0)
		return;

	repair_write_nthreads(fd, range_cnt);
	for (i = 0; i < range_cnt; i++) {
		repair_write_range(fd, range->rr_start, range->rr_end);
		range++;
	}
}

static int repair_range_generator(char *rangep[], struct repair_range *range)
{
	char *endptr;
	char *endptr2;
	int range_cnt;

	for (range_cnt = 0 ; *rangep; rangep++) {
		range->rr_start = strtol(*rangep, &endptr, 0);
		if (errno || *endptr != '-') {
			err_msg("invalid range: %c", *endptr);
			continue;
		}

		range->rr_end = strtol(&endptr[1], &endptr2, 0);
		if (errno || *endptr2) {
			err_msg("invalid range: %c", *endptr);
			continue;
		}
		range++;
		range_cnt++;
	}

	return range_cnt;
}

int main(int argc, char *argv[])
{
#define RANGE_MAX	128
	int fd;
	struct repair_range range[RANGE_MAX];
	int range_cnt = 0;

	if (argc < 3)
		usage(argv[0]);

	fd = repair_open(argv[1]);
	range_cnt = repair_range_generator(&argv[2], range);
	repair_doit(fd, range, range_cnt);
	repair_close(fd);

	return 0;
}
