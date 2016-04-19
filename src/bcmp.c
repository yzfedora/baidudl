/*
 * This is a tool for compare two binary files. and print the first mismatch
 * offset and difference.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sys/time.h>
#include <err_handler.h>

#define	BUFSZ		(1 << 20)
#define min(x, y)	((x < y) ? x : y)
#define max(x, y)	((x > y) ? x : y)

static ssize_t skip = 0;		/* size of skipped if specified. */
static ssize_t length = 128;		/* output most different bytes. */
static ssize_t offset = 0;
static ssize_t total = 0;
static int diff_found_flags = 0;
static int save_diff_flags = 0;
static int list_diff_flags = 0;

static ssize_t bindiff(const char *src, const char *dest, size_t count)
{
	const char *p1 = src;
	const char *p2 = dest;

	while (p1 < src + count) {
		/*printf("cmp %X vs %X\n", (unsigned char)*p1,
				(unsigned char)*p2);*/
		if (*p1 != *p2) {
			diff_found_flags = 1;
			break;
		}
		offset++;
		p1++;
		p2++;
	}
	return offset;
}

static ssize_t bindiff_list(const char *src, const char *dest, size_t count)
{
	const char *p1 = src;
	const char *p2 = dest;

	while (p1 < src + count) {
		if (*p1 != *p2) {
			printf("%-15ld %0X vs %0X\n", offset,
				(unsigned char)*p1, (unsigned char)*p2);
		}
		offset++;
		p1++;
		p2++;
	}
	return offset;
}

static ssize_t readn(int fd, void *buf, size_t count)
{
	ssize_t nrd;
	char *ptr = buf;
	size_t nleft = count;

	while (nleft > 0) {
		if ((nrd = read(fd, ptr, nleft)) > 0) {
			nleft -= nrd;
			ptr += nrd;
		} else if (nrd == 0) {
			return (count - nleft);
		} else {
			if (errno == EINTR)
				continue;
			err_sys("readn");
			return -1;
		}
	}
	return count;
}

static ssize_t writen(int fd, const void *buf, size_t count)
{
	int nwrt;
	const char *ptr = buf;
	size_t nleft = count;

	errno = 0;
	while (nleft > 0) {
		if ((nwrt = write(fd, ptr, nleft)) > 0) {
			nleft -= nwrt;
			ptr += nwrt;
		} else if (nwrt == 0) {
			return 0;
		} else {
			if (errno == EINTR)
				continue;
			err_sys("writen");
			return -1;
		}
	}
	return count;
}

static ssize_t binsize(int fd)
{
	off_t s = lseek(fd, 0, SEEK_END);
	if (s == (off_t)-1)
		err_sys("lseek");
	return s;
}

static ssize_t bincmp(int fd_src, int fd_dest)
{
	ssize_t src_read, dest_read;
	char src[BUFSZ];
	char dest[BUFSZ];

	while (1) {
		if ((src_read = readn(fd_src, src, sizeof(src))) <= 0) {
			if (src_read == 0)
				break;
			err_exit("compare at %ld", offset);
		}

		if ((dest_read = readn(fd_dest, dest, sizeof(dest))) <= 0) {
			if (dest_read == 0)
				break;
			err_exit("compare at %ld", offset);
		}
		
		if (src_read != dest_read) {
			if (list_diff_flags)
				bindiff_list(src, dest, min(src_read,
							dest_read));
			else
				bindiff(src, dest, min(src_read, dest_read));
			break;
		}

		if (list_diff_flags) {
			bindiff_list(src, dest, src_read);
		} else {
			bindiff(src, dest, src_read);	/* same bytes read. */
			if (diff_found_flags)
				break;		/* find mismatch location. */
		}
	}

	return offset;
}

/*
static void bindiff_print(int fd, off_t offset, size_t length)
{
	char buf[BUFSZ];
	char *ptr = buf;
	ssize_t nrd;
	int i;
	
	if (lseek(fd, offset, SEEK_SET) == (off_t)-1)
		err_sys("lseek");
	if ((nrd = readn(fd, buf, length)) > 0) {
		for (i = 0，length = min(nrd, length); i < length; i++) {
			printf("%#x", *ptr++);
			if (i % 8 == 0 && (i > 0))
				printf(" ");
			if (i % 80 == 0)
				printf("\n");
		}
	}
}*/

static void binskip(int fd, ssize_t skip)
{
	if (lseek(fd, skip, SEEK_SET) == -1)
		err_exit("set skip bytes %ld", skip);
}

/*
 * Save different data from start at offset to the end.
 */
static void binsave(int fd, const char *file)
{
#define	OPEN_FLAGS (O_WRONLY | O_CREAT | O_TRUNC)
	int fd_save;
	ssize_t nread;
	char buf[BUFSZ];
	char file_save[NAME_MAX + 1];

	snprintf(file_save, sizeof(file_save), "%s.diff", file);
	if ((fd_save = open(file_save, OPEN_FLAGS, 0640)) == -1)
		err_exit("open %s to save result", file_save);

	binskip(fd, offset);
	while ((nread = readn(fd, buf, sizeof(buf))) > 0 && length > 0) {
		if (nread > length)
			nread = length;
		if (writen(fd_save, buf, nread) <= 0) {
			break;
		}
		length -= nread;
	}
	close(fd_save);
}

static void dlinfo_sigalrm_handler(int signo)
{
	printf("\r""%*s", 80, " ");
	printf("\r""match progress %5.1f%%, offset: %ld",
			(offset * 100.0 / total), offset);
	fflush(stdout);
}

static void dlinfo_register_signal_handler(void)
{
	struct sigaction act, old;

	memset(&act, 0, sizeof(act));
	act.sa_flags |= SA_RESTART;
	act.sa_handler = dlinfo_sigalrm_handler;

	if (sigaction(SIGALRM, &act, &old) == -1)
		err_exit("sigaction - SIGALRM");
}


static void dlinfo_sigalrm_detach(void)
{
	if (setitimer(ITIMER_REAL, NULL, NULL) == -1)
		err_msg("setitimer");
}

static void dlinfo_alarm_launch(void)
{
	struct itimerval new;

	new.it_interval.tv_sec = 1;
	new.it_interval.tv_usec = 0;
	new.it_value.tv_sec = 1;
	new.it_value.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &new, NULL) == -1)
		err_msg("setitimer");
}
static void bincmp_run(const char *src, const char *dest)
{
	int fd_src;
	int fd_dest;
	
	if (!src || !dest)
		err_exit("compare files can't be NULL");

	if ((fd_src = open(src, O_RDONLY)) == -1)
		err_exit("open %s", src);
	if ((fd_dest = open(dest, O_RDONLY)) == -1)
		err_exit("open %s", dest);
	
	offset = skip;			/* plus skipped bytes. */
	total = min(binsize(fd_src), binsize(fd_dest));

	if (!list_diff_flags) {
		dlinfo_register_signal_handler();
		dlinfo_alarm_launch();
	}

	binskip(fd_src, skip);
	binskip(fd_dest, skip);
	bincmp(fd_src, fd_dest);

	if (save_diff_flags) {
		binsave(fd_src, src);
		binsave(fd_dest, dest);
	}
	
	if (!list_diff_flags) {
		int w = max(strlen(src), strlen(dest)) + 1;
		printf("\n%-*s %-15ld bytes\n", w, src, binsize(fd_src));
		printf("%-*s %-15ld bytes\n", w, dest, binsize(fd_dest));
		printf("diff at offset: %ld\n", offset);
	}

	dlinfo_sigalrm_detach();	
	if (close(fd_src) == -1)
		err_sys("close");
	if (close(fd_dest) == -1)
		err_sys("close");
}

static void binhelp(const char *progname)
{
	err_exit("Usage: %s [-Ssn] file1 file2\n"
		 "    -S        Output diff parts to suffix '.diff' file.\n"
		 "    -s offset Set the offset of where to start diff.\n"
		 "    -n length Set the match length from the offset.\n"
		 "    -l list   Find and list all mismatch.\n",
		progname);
}

int main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "Ss:n:l")) != -1) {
		switch (opt) {
		case 'S':
			save_diff_flags = 1;
			break;
		case 's':
			skip = atol(optarg);
			break;
		case 'n':
			length = atol(optarg);
			break;
		case 'l':
			list_diff_flags = 1;
			break;
		default:
			err_exit("Unknown option: %c", (char)opt);
		}
	}

	if (argc - optind < 2)
		binhelp(argv[0]);

	bincmp_run(argv[optind], argv[optind + 1]);
	return 0;	
}
