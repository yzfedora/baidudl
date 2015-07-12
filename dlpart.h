#ifndef _DLPART_H
#define _DLPART_H
#include <sys/types.h>
#include "dlinfo.h"

struct dlpart {
#define DLPART_BUFSZ	4096
	int	dp_remote;	/* remote server file descriptor */
	int	dp_local;	/* local file descriptor */
	ssize_t	dp_start;
	ssize_t	dp_end;
	char	dp_buf[DLPART_BUFSZ];
	int	dp_nrd;
	struct dlinfo *dp_info;

	void (*sendhdr)(struct dlpart *);
	int  (*recvhdr)(struct dlpart *);
	void (*read)(struct dlpart *, ssize_t *, ssize_t *);
	void (*write)(struct dlpart *);
	void (*delete)(struct dlpart *);
};

struct dlpart *dlpart_new(struct dlinfo *);
#endif
