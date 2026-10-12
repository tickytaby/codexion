#include "codexion.h"
#include "util.h"
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>

struct timespec	ms_to_ts(long ms)
{
	struct timespec	ts;

	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000;
	return (ts);
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
			break ;
		compile(c);
		release_dongles(c);
		debug(c);
		refactor(c);
	}
	return (NULL);
}
