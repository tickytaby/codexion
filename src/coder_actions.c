#include "util.h"
#include "codexion.h"

// Takes both dongles atomically: a coder never holds one while waiting
// for the other, so no hold-and-wait chains can form.
int	take_dongles(t_coder *c)
{
	t_table	*t;
	long	key;

	t = c->table;
	if (c->left == c->right)
	{
		log_action(c, "has taken a dongle");
		pthread_mutex_lock(&t->dongle_lock);
		while (!stopped(t))
			pthread_cond_wait(&t->dongle_cond, &t->dongle_lock);
		pthread_mutex_unlock(&t->dongle_lock);
		return (1);
	}
	key = queue_key(c);
	lock_and_enq(t, c, key);
	while (!stopped(t) && !(can_take(c, c->left) && can_take(c, c->right)))
		wait_dongles(c);
	if (stopped(t))
		return (pthread_mutex_unlock(&t->dongle_lock), 1);
	deq_and_unlock(t, c);
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
