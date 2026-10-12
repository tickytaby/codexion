#include <string.h>
#include "codexion.h"

long	queue_key(t_coder *c)
{
	long	last;

	pthread_mutex_lock(&c->lock);
	last = c->last_compile_start;
	pthread_mutex_unlock(&c->lock);
	if (!strcmp(c->table->cli_args.scheduler, "fifo"))
		return (now_ms());
	return (last + (long)c->table->cli_args.values[TIME_TO_BURNOUT]);
}

void	push(t_coder *c, t_dongle *d, long key)
{
	int	i;

	i = 0;
	if (d->queue[0].coder_id != -1)
		i = 1;
	d->queue[i].coder_id = c->id;
	d->queue[i].key = key;
	d->queue[i].ticket = c->table->ticket++;
}

// Orders by key, then by arrival ticket so equal keys are served
// first-come first-served (a strict total order: no circular waits).
void	heapify(t_waiter *q)
{
	t_waiter	tmp;

	tmp.coder_id = -1;
	tmp.key = 0;
	tmp.ticket = 0;
	if (q[0].coder_id != -1 && q[1].coder_id != -1)
	{
		if (q[0].key > q[1].key
			|| (q[0].key == q[1].key && q[0].ticket > q[1].ticket))
		{
			tmp = q[1];
			q[1] = q[0];
			q[0] = tmp;
		}
	}
	else if (q[0].coder_id == -1 && q[1].coder_id != -1)
	{
		q[0] = q[1];
		q[1] = tmp;
	}
}

void	enq(t_coder *c, t_dongle *d, long key)
{
	push(c, d, key);
	heapify(d->queue);
}

// Removes coder_id's entry, which is not necessarily the head
void	deq(t_dongle *d, int coder_id)
{
	int	i;

	i = -1;
	while (++i < 2)
	{
		if (d->queue[i].coder_id == coder_id)
		{
			d->queue[i].coder_id = -1;
			d->queue[i].key = 0;
			d->queue[i].ticket = 0;
		}
	}
	heapify(d->queue);
}
