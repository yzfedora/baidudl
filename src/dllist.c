#include <unistd.h>
#include <stdlib.h>
#include "dllist.h"

struct dllist {
	pid_t		pid;
	void		*data;
	struct dllist	*next;
};

static struct dllist *list_head, *list_last;

/*
 * We use a list to store the struct dlinfo, because when a signal is arrived.
 * there is no way to get the struct dlinfo from the handler, we must use a
 * global static structure to store it, and retrieve it via it's pid.
 */
int dllist_put(void *data)
{
	pid_t pid = getpid();
	struct dllist *new = calloc(1, sizeof(*new));

	if (!new)
		return -1;

	new->pid = pid;
	new->data = data;

	if (!list_head) {
		list_head = new;
		list_last = new;
	} else {
		list_last->next = new;
		list_last = list_last->next;
	}

	return 0;
}

void *dllist_get(void)
{
	pid_t pid = getpid();
	struct dllist *t = list_head;

	while (t) {
		if (t->pid == pid)
			return t->data;

		t = t->next;
	}

	return NULL;
}
