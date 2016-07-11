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
#ifndef _ERR_HANDLER_H
#define _ERR_HANDLER_H
#include <errno.h>

/* #undef __GNUC__ */
#ifdef __GNUC__
#define likely(x)	__builtin_expect((x), 1)
#define unlikely(x)	__builtin_expect((x), 0)
#else
#define likely(x)	(x)
#define unlikely(x)	(x)
#endif

typedef enum { false = 0, true = 1 } bool;

void err_setdebug(int level);
void err_setdaemon(bool flags);
void err_setout(int fd);
void err_dbg(int level, const char *msg, ...)
				__attribute__((format(printf, 2, 3)));
void err_msg(const char *msg, ...) __attribute__((format(printf, 1, 2)));
void err_sys(const char *msg, ...) __attribute__((format(printf, 1, 2)));
void err_exit(const char *msg, ...) __attribute__((noreturn,
			format(printf, 1, 2)));
#endif
