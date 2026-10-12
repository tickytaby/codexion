#include <sys/time.h>
#include <unistd.h>
#include "codexion.h"

long	now_ms(void)
{
	struct timeval	time;

	gettimeofday(&time, NULL);
	return (time.tv_sec * 1000L + time.tv_usec / 1000);
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
