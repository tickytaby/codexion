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
	if (d->queue[0].coder_id == -1)
	{
		d->queue[0].coder_id = c->id;
		d->queue[0].key = key;
		return ;
	}
		d->queue[1].coder_id = c->id;
		d->queue[1].key = key;
}

void	heapify(t_waiter *q)
{
	t_waiter	tmp;

	tmp.coder_id = -1;
	tmp.key = 0;
	if (q[0].coder_id != -1 && q[1].coder_id != -1)
	{
		if (q[0].key > q[1].key)
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

void	deq(t_dongle *d)
{
	t_waiter	tmp;	

	tmp.coder_id = -1;
	tmp.key = 0;
	d->queue[0] = tmp;
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

int	can_take(t_coder *c, t_dongle *d)
{
	int	first_pos;
	int	free;

	first_pos = (peek(d->queue)->coder_id == c->id);
	free = !d->in_use && d->available_at <= now_ms();
	return (first_pos && free);
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

int take_one(t_coder *c, t_dongle *d, long key)
{
	struct timespec	ts;

	enq(c, d, key);
	while (!stopped(c->table) && !can_take(c, d))
	{
		if (!d->in_use && now_ms() < d->available_at)
		{
			ts = ms_to_ts(d->available_at);
			pthread_cond_timedwait(&d->cond, &d->lock, &ts);
		}
		else
			pthread_cond_wait(&d->cond, &d->lock);
	}
	deq(d);
	d->in_use = 1;
	log_action(c, "has taken a dongle");
	return (1);
}

int	take_dongles(t_coder *c)
{
	t_dongle	*first;
	t_dongle	*second;
	long		key;

	if (c->left == c->right)
		return (1);
	first = c->right;
	second = c->left;
	if (c->left->id > c->right->id)
	{
		first = c->left;
		second = c->right;
	}
	key = queue_key(c);
	pthread_mutex_lock(&first->lock);
	take_one(c, first, key);
	pthread_mutex_unlock(&first->lock);
	pthread_mutex_lock(&second->lock);
	take_one(c, second, key);
	pthread_mutex_unlock(&second->lock);

	return (0);	
}

void	release_dongles(t_coder *c)
{
	pthread_mutex_lock(&c->left->lock);
	c->left->in_use = 0;
	c->left->available_at = now_ms() + c->table->cli_args.values[DONGLE_COOLDOWN];
	pthread_mutex_unlock(&c->left->lock);
	pthread_cond_broadcast(&c->left->cond);
	pthread_mutex_lock(&c->right->lock);
	c->right->in_use = 0;
	c->right->available_at = now_ms() + c->table->cli_args.values[DONGLE_COOLDOWN];
	pthread_mutex_unlock(&c->right->lock);
	pthread_cond_broadcast(&c->right->cond);
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
