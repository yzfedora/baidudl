/* This is a multi-thread download tool, for pan.baidu.com
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
#ifndef _ERR_HANDLE_H
#define _ERR_HANDLE_H
#include <errno.h>

#define ERR_EXIT	01
#define ERR_THREAD_EXIT	02

#define err_exit(err, fmt, ...)	\
	_err_exit(ERR_EXIT, err, "[%s: %d] "fmt, __FILE__,\
		__LINE__, ##__VA_ARGS__)
#define err_thread_exit(err, fmt, ...)\
	_err_exit(ERR_THREAD_EXIT, err, "[%s: %d] "fmt, __FILE__,\
		__LINE__, ##__VA_ARGS__)
#define err_msg(err, fmt, ...)	\
	_err_exit(0, err, "[%s: %d] "fmt, __FILE__, __LINE__, ##__VA_ARGS__)

#ifdef __DEBUG__
#define debug(fmt, ...)	\
	fprintf(stderr, "[%s: %d] "fmt, __FILE__, __LINE__,\
			##__VA_ARGS__);
#else
#define debug(fmt, ...)
#endif

void _err_exit(int flags, int err, char *fmt, ...);
#endif
