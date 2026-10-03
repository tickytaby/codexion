#include "codexion.h"
#include <unistd.h>


int	should_stop(t_table *t, t_coder *c, int num_coders)
{
	int	i;
	int	count;

	i = 0;
	count = 0;
	while (i < num_coders)
	{
		// We need to acquire lock before reading.
		if (now_ms() >= c[i].last_compile_start + t->cli_args.values[TIME_TO_BURNOUT])
			return (2);
		if (c[i++].compiles_done >= t->cli_args.values[COMPILES_REQUIRED])
			count++;
	}
	if (count == num_coders)
		return (1);
	return (0);
	// need to return also who burns out.
}

void	*monitor(void *arg)
{
	// Only writer to stop, checks for # of compiles_done && burnout
	t_table	*t;
	int		cond;

	t = (t_table *)arg;
	cond = 0;
	while (!cond)
	{
		cond = should_stop(t, t->coders, t->cli_args.values[NUM_CODERS]);
		if (!cond)
			usleep(1000);
	}
	pthread_mutex_lock(&t->stop_lock);
	t->stop= 1;
	pthread_mutex_unlock(&t->stop_lock);

	return (NULL);
}
