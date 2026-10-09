#include "codexion.h"
#include "parser.h"
#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

long	now_ms(void)
{
	struct timeval	time;

	gettimeofday(&time, NULL);
	return (time.tv_sec * 1000L + time.tv_usec / 1000);
}

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

void	heapify(t_waiter *q)
{
	// Orders by key, then by arrival ticket so equal keys are served
	// first-come first-served (a strict total order: no circular waits).
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

void	deq(t_dongle *d, int coder_id)
{
	// Removes coder_id's entry, which is not necessarily the head
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

t_waiter	*peek(t_waiter *q)
{
	return (&q[0]);
}

int	stopped(t_table *t)
{
	int	out;
	
	out = 0;
	pthread_mutex_lock(&t->stop_lock);
	out = t->stop;
	pthread_mutex_unlock(&t->stop_lock);
	return (out);
}

int	sim_sleep(t_table *t, long ms)
{
	long	end;
	long	remaining;

	end = now_ms() + ms;
	while (!stopped(t))
	{
		remaining = end - now_ms();
		if (remaining <= 0)
			return (0);
		if (remaining > 1)
			usleep((remaining - 1) * 1000);
		else
			usleep(200);
	}
	return (1);
}

t_dongle	*other_dongle(t_coder *c, t_dongle *d)
{
	if (c->left == d)
		return (c->right);
	return (c->left);
}

int	can_take(t_coder *c, t_dongle *d)
{
	// d must be free. If c is not first in d's queue, the head (the
	// neighbour sharing d) only keeps d reserved when it is also first in
	// line for its other dongle, i.e. it is really next. Otherwise it would
	// hold d hostage while waiting, which recreates hold-and-wait chains.
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

struct timespec ms_to_ts(long ms)
{
	struct timespec	ts;

	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000;
	return (ts);
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

void	wait_dongles(t_coder *c)
{
	// Called with dongle_lock held. If both dongles are free but one is still
	// cooling down, sleep until the later cooldown ends; otherwise wait for a
	// release (or the monitor's stop broadcast).
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

int	take_dongles(t_coder *c)
{
	// Takes both dongles atomically: a coder never holds one while waiting
	// for the other, so no hold-and-wait chains can form.
	t_table	*t;
	long	key;

	t = c->table;
	if (c->left == c->right)
	{
		// Lone coder: only one dongle exists, so it can never compile.
		// Hold it and wait for the monitor to declare the burnout.
		log_action(c, "has taken a dongle");
		pthread_mutex_lock(&t->dongle_lock);
		while (!stopped(t))
			pthread_cond_wait(&t->dongle_cond, &t->dongle_lock);
		pthread_mutex_unlock(&t->dongle_lock);
		return (1);
	}
	key = queue_key(c);
	pthread_mutex_lock(&t->dongle_lock);
	enq(c, c->left, key);
	enq(c, c->right, key);
	pthread_cond_broadcast(&t->dongle_cond);
	while (!stopped(t) && !(can_take(c, c->left) && can_take(c, c->right)))
		wait_dongles(c);
	if (stopped(t))
		return (pthread_mutex_unlock(&t->dongle_lock), 1);
	deq(c->left, c->id);
	deq(c->right, c->id);
	c->left->in_use = 1;
	c->right->in_use = 1;
	pthread_cond_broadcast(&t->dongle_cond);
	pthread_mutex_unlock(&t->dongle_lock);
	log_action(c, "has taken a dongle");
	log_action(c, "has taken a dongle");
	return (0);
}

void	release_dongles(t_coder *c)
{
	t_table	*t;

	t = c->table;
	pthread_mutex_lock(&t->dongle_lock);
	c->left->in_use = 0;
	c->right->in_use = 0;
	c->left->available_at = now_ms() + t->cli_args.values[DONGLE_COOLDOWN];
	c->right->available_at = now_ms() + t->cli_args.values[DONGLE_COOLDOWN];
	pthread_cond_broadcast(&t->dongle_cond);
	pthread_mutex_unlock(&t->dongle_lock);
}

int	compile(t_coder *c)
{
	pthread_mutex_lock(&c->lock);
	c->last_compile_start = now_ms();
	pthread_mutex_unlock(&c->lock);
	log_action(c, "is compiling");
	if (sim_sleep(c->table, c->table->cli_args.values[TIME_TO_COMPILE]))
		return (0);
	pthread_mutex_lock(&c->lock);
	c->compiles_done++;
	pthread_mutex_unlock(&c->lock);
	return (1);
}

void	refactor(t_coder *c)
{
	log_action(c, "is refactoring");
	sim_sleep(c->table, c->table->cli_args.values[TIME_TO_REFACTOR]);
}

void	debug(t_coder *c)
{
	log_action(c, "is debugging");
	sim_sleep(c->table, c->table->cli_args.values[TIME_TO_DEBUG]);
}

void	*coder_routine(void *arg)
{
	t_coder	*c;

	c = (t_coder *)arg;
	pthread_mutex_lock(&c->table->start_lock);
	while (!c->table->start)
		pthread_cond_wait(&c->table->start_cond, &c->table->start_lock);
	pthread_mutex_unlock(&c->table->start_lock);
	while (!stopped(c->table))
	{
		if (take_dongles(c))
			break;
		compile(c);
		release_dongles(c);
		debug(c);
		refactor(c);
	}
	return (NULL);
}
