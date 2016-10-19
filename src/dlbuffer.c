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
#include "dlbuffer.h"
#include <stdlib.h>
#include <string.h>

static int dlbuffer_increase(struct dlbuffer *db)
{
	char *buf;
	size_t len = db->len + DLBUFFER_INCREASE_SIZE;

	buf = realloc(db->buf, len);

	if (!buf)
		return -1;

	db->buf = buf;
	db->len = len;
	return 0;
}

int dlbuffer_write(struct dlbuffer *db, void *buf, size_t size)
{
	char *ptr = (char *)buf;

	while (db->len - db->pos < size) {
		if (dlbuffer_increase(db) < 0)
			return -1;
	}

	memcpy(db->buf + db->pos, ptr, size);
	db->pos += size;
	return 0;
}

void dlbuffer_free(struct dlbuffer *db)
{
	if (!db)
		return;
	if (db->buf)
		free(db->buf);
	free(db);
}

struct dlbuffer *dlbuffer_new(void)
{
	struct dlbuffer *db = calloc(1, sizeof(*db));

	if (!db)
		return NULL;

	return db;
}
