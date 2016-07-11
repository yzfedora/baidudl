/* This is a general error process library for linux in C
 *   		Copyright (C) 2015  Yang Zhang <yzfedora@gmail.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *************************************************************************/
/* #define LIBERR_TEST */	/* For liberr stop and cont test. */
#define _DEFAULT_SOURCE		/* vsyslog() */
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include "err_handler.h"

#ifdef	LIBERR_TEST
# include <sys/stat.h>
# include <fcntl.h>
#endif


#define ERR_BUFFER	4096
#define COLOR_RST	"\e[0m"		/* Reset color of terminal. */
#define COLOR_RED	"\e[31m"	/* Set red font. */


#ifdef	LIBERR_TEST
# define TEST_OUT	"test.out"
# define TEST_SYM_TSTP	"#TSTP#"
# define TEST_SYM_CONT	"#CONT#"
#endif

#define call_err_internal(doexit, doerr, level, msg)		\
	do {							\
		va_list ap;					\
		va_start(ap, msg);				\
		err_internal(doexit, doerr, level, msg, ap);	\
		va_end(ap);					\
	} while (0)

/* For valid set color of terminal */
#define DO_COLOR_SET (!_err_daemon && _err_tty)
/* The 'x' is a macro in the one of COLOR_*. */
#define COLOR_SET(x) (write(STDERR_FILENO, x, sizeof(x) - 1) != (sizeof(x) - 1))

static int _err_debug_level = 0;
static int _err_daemon = 0;
static int _err_tty = 0;

#ifdef	LIBERR_TEST
static int tstp_received;
static int cont_received;
#endif

void err_setdebug(int level)
{
	_err_debug_level = level;
}

void err_setdaemon(bool flags)
{
	if (flags)	_err_daemon = 1;
	else		_err_daemon = 0;
}

void err_setout(int fd)
{
	if (dup2(fd, STDERR_FILENO) == -1)
		err_sys("liberr redirect stderr");
	_err_tty = isatty(STDERR_FILENO);
}

/* 
 * Do some cleanup when program exit normally, this mean not killed by signal.
 */
__attribute__((destructor(255))) void err_fini(void)
{
#ifdef	LIBERR_TEST
	char buf[32];
	size_t len;
	int fd = open(TEST_OUT, O_WRONLY | O_CREAT | O_TRUNC, 0640);
	if (fd == -1) {
		err_sys("open %s for write test statistics", TEST_OUT);
		goto out;
	}
	snprintf(buf, 32, "%d %d", tstp_received, cont_received);
	len = strlen(buf);
	if (write(fd, buf, len) != len) {
		err_sys("write test statistics to %s", TEST_OUT);
		goto out;
	}
out:
	if (fd > 0)
		close(fd);
#endif
	closelog();

	/* Reset color of font in the terminal */
	if (DO_COLOR_SET) {
		if (COLOR_SET(COLOR_RST))
			err_sys("liberr restore color to default error");
	}
}

/*
 * This situation is program killed by signal, do some cleanup.
 */
static void err_general_sighandler(int signo)
{
	err_fini();
	raise(signo);
}

/*
 * liberr caught signal of stop, ensure color set to default.
 */
static void err_sigtstp_handler(int signo)
{
	if (DO_COLOR_SET) {
		if (COLOR_SET(COLOR_RST))
			err_sys("liberr restore color to default error");
	}
#ifdef	LIBERR_TEST
	tstp_received++;
	if (COLOR_SET(TEST_SYM_TSTP));
#endif
	raise(SIGSTOP);
}

/*
 * liberr awake by signal of continue, ensure color is set to red. (for
 * err_sys() and err_exit() special)
 */
static void err_sigcont_handler(int signo)
{
	if (DO_COLOR_SET) {
		if (COLOR_SET(COLOR_RED))
			err_sys("liberr restore color to red error");
	}

	/* 
	 * Use to test the restore the color, when program received the signal
	 * SIGTSTP or SIGCONT, because may couldn't output a entire error when
	 * signal arrived (which start with: \e[31m and end with \e[0m).
	 */
#ifdef	LIBERR_TEST
	cont_received++;
	if (COLOR_SET(TEST_SYM_CONT));
#endif
}

__attribute__((constructor(255))) void err_init(void)
{
	struct sigaction act, oact;
	
	_err_tty = isatty(STDERR_FILENO);
	openlog(NULL, LOG_NDELAY | LOG_PID, LOG_USER);

	sigemptyset(&act.sa_mask);
	act.sa_handler = err_general_sighandler;
	act.sa_flags = SA_RESETHAND;
	if (sigaction(SIGHUP, &act, &oact) == -1)
		err_sys("liberr set signal handler for SIGHUP error");
	if (sigaction(SIGINT, &act, &oact) == -1)
		err_sys("liberr set signal handler for SIGINT error");
	if (sigaction(SIGQUIT, &act, &oact) == -1)
		err_sys("liberr set signal handler for SIGQUIT error");
	if (sigaction(SIGABRT, &act, &oact) == -1)
		err_sys("liberr set signal handler for SIGABRT error");
	if (sigaction(SIGSEGV, &act, &oact) == -1)
		err_sys("liberr set signal handler for SIGSEGV error");
	if (sigaction(SIGTERM, &act, &oact) == -1)
		err_sys("liberr set signal handler for SIGTERM error");
	
	act.sa_handler = err_sigtstp_handler;
	act.sa_flags = 0;
	if (sigaction(SIGTSTP, &act, &oact) == -1)
		err_sys("liberr set signal handler for SIGTSTP error");
	act.sa_handler = err_sigcont_handler;
	if (sigaction(SIGCONT, &act, &oact) == -1)
		err_sys("liberr set signal handler for SIGCONT error");
}

static void err_internal(bool doexit, bool doerr, int level,
		const char *msg, va_list ap)
{
	int __errno = errno;	/* Saved for asynchronous signal handler. */

	size_t len = 0;		/* For optimize times of call strlen() */
	char buf[ERR_BUFFER];

	if (doerr && DO_COLOR_SET) {
		strncpy(buf, COLOR_RED, sizeof(buf));
		len += sizeof(COLOR_RED) - 1;
	}

	vsnprintf(buf + len, sizeof(buf) - len, msg, ap);
	/* Append ": " and error string to the end of 'buf'. */
	if (doerr && __errno) {
		len = strlen(buf);
		strncat(buf + len, ": ", sizeof(buf) - len);
		len += 2;
		strerror_r(__errno, buf + len, sizeof(buf) - len);
	}

	/* Append "\n" to the end of 'buf'. */
	len += strlen(buf + len);
	strncat(buf + len, "\n", sizeof(buf) - len);
	len += 1;

	if (doerr && DO_COLOR_SET) {
		strncat(buf + len, COLOR_RST, sizeof(buf) - len);
		len += sizeof(COLOR_RST) - 1;
	}

	if (!_err_daemon)
		write(STDERR_FILENO, buf, len);
	else
		syslog(level, buf);

	if (doexit)
		exit(EXIT_FAILURE);
}

void err_dbg(int level, const char *msg, ...)
{
	if (level > _err_debug_level)
		return;

	call_err_internal(false, false, LOG_DEBUG, msg);
}

void err_msg(const char *msg, ...)
{
	call_err_internal(false, false, LOG_INFO, msg);
}

void err_sys(const char *msg, ...)
{
	call_err_internal(false, true, LOG_ERR, msg);
}

void err_exit(const char *msg, ...)
{
	call_err_internal(true, true, LOG_ERR, msg);
	/* 
	 * Force to tell compiler this is 'noreturn', because of in the macro
	 * 'call_err_internal', the first argument 'doexit' is true.
	 */
	__builtin_unreachable();
}
