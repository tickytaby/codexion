#include "codexion.h"
#include "parser.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>


void	should_stop(t_table *t, t_coder *c, int num_coders, int *cond)
{
	// modifies cond[2]
	// -> cond[0] is the exit condition (2 -> burnout, cond[1] coder_id)
	// 									(1 -> required compiles done)
	// 									(0 -> should not stop yet)
	int	i;
	int	count;

	i = 0;
	count = 0;
	while (i < num_coders)
	{
		pthread_mutex_lock(&c[i].lock);
		if (now_ms() >= c[i].last_compile_start + t->cli_args.values[TIME_TO_BURNOUT])
		{
			cond[0] = 2;
			cond[1] = i;
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

void	*monitor_routine(void *arg)
{
	// Only writer to stop, checks for # of compiles_done && burnout
	t_table	*t;
	int		cond[2];

	t = (t_table *)arg;
	memset(cond, 0, sizeof(int) * 2);
	while (!cond[0])
	{
		should_stop(t, t->coders, t->cli_args.values[NUM_CODERS], cond);
		if (cond[0])
			break;
		if (!cond[0])
			usleep(1000);
	}
	pthread_mutex_lock(&t->print_lock);
	printf("cond[0] = %d, cond[1] = %d\n", cond[0], cond[1]);
	int i = -1;
	while (++i < t->cli_args.values[NUM_CODERS])
		printf("Coder #%d -> %d compiles done\n", i + 1, t->coders[i].compiles_done);
	pthread_mutex_unlock(&t->print_lock);
	pthread_mutex_lock(&t->stop_lock);
	t->stop= 1;
	pthread_mutex_unlock(&t->stop_lock);
	if (cond[0] == 2)
		log_action(&t->coders[cond[1]], "burned out");
	else
	{
		pthread_mutex_lock(&t->print_lock);
		printf("All required compiles done\n");
		pthread_mutex_unlock(&t->print_lock);
	}

	return (NULL);
}
