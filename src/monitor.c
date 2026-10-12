#include "codexion.h"
#include "parser.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>

// modifies cond[2]
// -> cond[0] is the exit condition (2 -> burnout, cond[1] coder_id)
// 									(1 -> required compiles done)
// 									(0 -> should not stop yet)
void	should_stop(t_table *t, t_coder *c, int num_coders, int *cond)
{
	int	i;
	int	count;

	i = 0;
	count = 0;
	while (i < num_coders)
	{
		pthread_mutex_lock(&c[i].lock);
		if (now_ms() >= c[i].last_compile_start
			+ t->cli_args.values[TIME_TO_BURNOUT])
		{
			cond[0] = 2;
			cond[1] = i;
			pthread_mutex_unlock(&c[i].lock);
			return ;
		}
		if (c[i].compiles_done >= t->cli_args.values[COMPILES_REQUIRED])
			count++;
		pthread_mutex_unlock(&c[i].lock);
		i++;
	}
	if (count == num_coders)
		cond[0] = 1;
}

void	set_stop_cond(t_table *t)
{
	pthread_mutex_lock(&t->stop_lock);
	t->stop = 1;
	pthread_mutex_unlock(&t->stop_lock);
	pthread_mutex_lock(&t->dongle_lock);
	pthread_cond_broadcast(&t->dongle_cond);
	pthread_mutex_unlock(&t->dongle_lock);
}

void	print_burnout_msg(t_table *t, int *cond)
{
	if (cond[0] == 2)
	{
		pthread_mutex_lock(&t->print_lock);
		printf("%ld %d burned out\n",
			now_ms() - t->start_time,
			t->coders[cond[1]].id + 1);
		pthread_mutex_unlock(&t->print_lock);
	}
}

// Only writer to stop, checks for # of compiles_done && burnout
void	*monitor_routine(void *arg)
{
	t_table	*t;
	int		cond[2];

	t = (t_table *)arg;
	pthread_mutex_lock(&t->start_lock);
	while (!t->start)
		pthread_cond_wait(&t->start_cond, &t->start_lock);
	pthread_mutex_unlock(&t->start_lock);
	if (stopped(t))
		return (NULL);
	memset(cond, 0, sizeof(int) * 2);
	while (!cond[0])
	{
		should_stop(t, t->coders, t->cli_args.values[NUM_CODERS], cond);
		if (cond[0])
			break ;
		if (!cond[0])
			usleep(1000);
	}
	set_stop_cond(t);
	print_burnout_msg(t, cond);
	return (NULL);
}
