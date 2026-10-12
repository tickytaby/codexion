#include "codexion.h"
#include "util.h"
#include <stdio.h>

int	can_take(t_coder *c, t_dongle *d)
{
	t_coder	*head;
	int		free;

	free = !d->in_use && d->available_at <= now_ms();
	if (!free)
		return (0);
	if (peek(d->queue)->coder_id == c->id)
		return (1);
	head = &c->table->coders[peek(d->queue)->coder_id];
	return (peek(other_dongle(head, d)->queue)->coder_id != head->id);
}

void	log_action(t_coder *c, char *action)
{
	long	t;

	pthread_mutex_lock(&c->table->print_lock);
	t = now_ms() - c->table->start_time;
	if (!stopped(c->table))
		printf("%ld %d %s\n", t, c->id + 1, action);
	pthread_mutex_unlock(&c->table->print_lock);
}

// Called with dongle_lock held. If both dongles are free but one is still
// cooling down, sleep until the later cooldown ends; otherwise wait for a
// release (or the monitor's stop broadcast).
void	wait_dongles(t_coder *c)
{
	struct timespec	ts;
	long			wake;

	wake = c->left->available_at;
	if (c->right->available_at > wake)
		wake = c->right->available_at;
	if (!c->left->in_use && !c->right->in_use && now_ms() < wake)
	{
		ts = ms_to_ts(wake);
		pthread_cond_timedwait(&c->table->dongle_cond,
			&c->table->dongle_lock, &ts);
	}
	else
		pthread_cond_wait(&c->table->dongle_cond, &c->table->dongle_lock);
}

void	lock_and_enq(t_table *t, t_coder *c, long key)
{
	pthread_mutex_lock(&t->dongle_lock);
	enq(c, c->left, key);
	enq(c, c->right, key);
	pthread_cond_broadcast(&t->dongle_cond);
}

void	deq_and_unlock(t_table *t, t_coder *c)
{
	deq(c->left, c->id);
	deq(c->right, c->id);
	c->left->in_use = 1;
	c->right->in_use = 1;
	pthread_cond_broadcast(&t->dongle_cond);
	pthread_mutex_unlock(&t->dongle_lock);
}
