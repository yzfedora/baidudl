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
#ifndef _DLCOMMON_H
#define _DLCOMMON_H
#include "dlinfo.h"

int getrcode(char *s);
int url_is_http(const char *url);
void get_filename_from_url(struct dlinfo *dl);
int getwcol(void);
ssize_t writen(int fd, const void *buf, size_t count);
char *string_decode(char *src);
char *dlstrcasestr(const char *haystack, const char *needle);
#endif
