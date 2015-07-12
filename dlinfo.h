#ifndef _DLINFO_H
#define _DLINFO_H
#include <sys/types.h>
#include <limits.h>

#define	DLINFO_REQ_SZ	4096
#define DLINFO_RCV_SZ	(1024 * 128)
#define DLINFO_SRV_SZ	64
#define DLINFO_HST_SZ	512
#define DLINFO_URL_SZ	4096
#define DLINFO_NAME_MAX	(NAME_MAX + 1)

struct dlinfo {
	int	di_remote;
	int	di_local;
	int	di_nthread;	/* number of threads to download */
	ssize_t	di_length;	/* total length of the file */

	char	di_filename[DLINFO_NAME_MAX];
	char	di_serv[DLINFO_SRV_SZ];	/* service type */
	char	di_host[DLINFO_HST_SZ];	/* host name or IP address */
	char	di_url[DLINFO_URL_SZ];	/* oritinal download url request */
	
	int  (*connect)(struct dlinfo *);/* used by dlpart_new */
	void (*setprompt)(struct dlinfo *);
	void (*rgstralrm)(void);
	void (*launch)(struct dlinfo *);
	void (*delete)(struct dlinfo *);
};

struct dlinfo *dlinfo_new(char *url, int nthread);
#endif
