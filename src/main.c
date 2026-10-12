#include <stdio.h>
#include <limits.h>
#include "util.h"
#include "parser.h"
#include "codexion.h"

void	release_start(t_table *t, int failed)
{
	int	i;

	if (failed)
	{
		pthread_mutex_lock(&t->stop_lock);
		t->stop = 1;
		pthread_mutex_unlock(&t->stop_lock);
	}
	pthread_mutex_lock(&t->start_lock);
	t->start = 1;
	t->start_time = now_ms();
	i = -1;
	while (++i < t->cli_args.values[NUM_CODERS])
	{
		pthread_mutex_lock(&t->coders[i].lock);
		t->coders[i].last_compile_start = t->start_time;
		pthread_mutex_unlock(&t->coders[i].lock);
	}
	pthread_cond_broadcast(&t->start_cond);
	pthread_mutex_unlock(&t->start_lock);
}

int	create_coders(t_table *t)
{
	int	created;

	created = 0;
	while (created < t->cli_args.values[NUM_CODERS]
		&& !pthread_create(&t->coders[created].thread, NULL, &coder_routine,
			&t->coders[created]))
		created++;
	return (created);
}

void	join_threads(t_table *t, int created, pthread_t monitor)
{
	int	i;

	i = -1;
	while (++i < created)
		pthread_join(t->coders[i].thread, NULL);
	pthread_join(monitor, NULL);
}

int	main(int argc, char *argv[])
{
	t_cliArgsValidation	args;
	t_table				t;
	int					created;
	pthread_t			monitor;

	args = validate_cli_args(argc, argv);
	if (args.error)
		return (display_args(args), 1);
	if (init_table(&t, args.cli_args))
		return (destroy_table(&t), 1);
	if (pthread_create(&monitor, NULL, &monitor_routine, &t))
		return (printf("Error: failed to create monitor thread\n"),
			destroy_table(&t), 1);
	created = create_coders(&t);
	release_start(&t, created < t.cli_args.values[NUM_CODERS]);
	join_threads(&t, created, monitor);
	destroy_table(&t);
	if (created < t.cli_args.values[NUM_CODERS])
		return (printf("Error: failed to create coder thread\n"), 1);
	return (0);
}
