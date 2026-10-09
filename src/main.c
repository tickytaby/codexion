#include "parser.h"
#include "codexion.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

int	init_dongles(t_table *t, int n)
{
	int i;
	
	i = 0;
	memset(t->dongles, 0, sizeof(t_dongle) * n);
	if (pthread_mutex_init(&t->dongle_lock, NULL))
		return (1);
	t->books.dongle_lock_ready++;
	if (pthread_cond_init(&t->dongle_cond, NULL))
		return (1);
	t->books.dongle_cond_ready++;
	while (i < n)
	{
		t->dongles[i].queue[0].coder_id = -1;
		t->dongles[i].queue[1].coder_id = -1;
		t->dongles[i].id = i;
		i++;
	}
	return (0);
}

int init_coders(t_table *t, int n)
{
	int i;

	i = 0;
	memset(t->coders, 0, sizeof(t_coder) * n);
	while (i < n)
	{
		if (pthread_mutex_init(&t->coders[i].lock, NULL))
			return (1);
		t->coders[i].id = i;
		t->coders[i].left = &t->dongles[i];
		t->coders[i].right = &t->dongles[(i + 1) % n];
		t->coders[i].table = t;
		t->books.coders_ready++;
		i++;
	}
	return (0);
}

int	init_table(t_table *t, t_cliArgs args)
{
	int res[2];

	memset(t, 0, sizeof(t_table));
	if (pthread_mutex_init(&t->print_lock, NULL))
		return (1);
	t->books.print_lock_ready++;
	if (pthread_mutex_init(&t->start_lock, NULL)) 
		return (1);
	t->books.start_lock_ready++;
	if (pthread_cond_init(&t->start_cond, NULL))
		return (1);
	t->books.start_cond_ready++;
	if (pthread_mutex_init(&t->stop_lock, NULL))
		return (1);
	t->books.stop_lock_ready++;
	t->cli_args = args;
	t->coders = malloc(sizeof(t_coder) * args.values[NUM_CODERS]);
	if (!t->coders)
		return (1);
	t->dongles = malloc(sizeof(t_dongle) * args.values[NUM_CODERS]);
	if (!t->dongles)
		return (1);
	res[0] = init_dongles(t, args.values[NUM_CODERS]);
	res[1] = init_coders(t, args.values[NUM_CODERS]);
	if (res[0] || res[1])
		return (1);
	return (0);
}

void	destroy_table(t_table *t)
{
	while (t->books.coders_ready-- > 0)
		pthread_mutex_destroy(&t->coders[t->books.coders_ready].lock);
	if (t->books.dongle_lock_ready)
		pthread_mutex_destroy(&t->dongle_lock);
	if (t->books.dongle_cond_ready)
		pthread_cond_destroy(&t->dongle_cond);
	if (t->books.start_cond_ready)
		pthread_cond_destroy(&t->start_cond);
	free(t->dongles);
	free(t->coders);
	if (t->books.print_lock_ready)
		pthread_mutex_destroy(&t->print_lock);
	if (t->books.start_lock_ready)
		pthread_mutex_destroy(&t->start_lock);
	if (t->books.stop_lock_ready)
		pthread_mutex_destroy(&t->stop_lock);
}

void	display_table(t_table *t)
{
	int	i;

	i = 0;
	while (i < t->cli_args.values[NUM_CODERS])
	{
		printf("Coder #%d -> left_dongle #%d, right_dongle #%d\n", t->coders[i].id, t->coders[i].left->id, t->coders[i].right->id);
		i++;
	}
}

void	release_start(t_table *t, int failed)
{
	// Opens the start barrier. If thread creation failed, stop is set first
	// so every thread already created exits as soon as it wakes up.
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

int	main(int argc, char *argv[])
{
	t_cliArgsValidation	args;
	t_table				t;
	int					created;
	int					i;
	pthread_t			monitor;

	args = validate_cli_args(argc, argv);
	if (args.error)
		return (display_args(args), 1);
	if (init_table(&t, args.cli_args))
		return (destroy_table(&t), 1);
	if (pthread_create(&monitor, NULL, &monitor_routine, &t))
		return (printf("Error: failed to create monitor thread\n"),
			destroy_table(&t), 1);
	created = 0;
	while (created < t.cli_args.values[NUM_CODERS]
		&& !pthread_create(&t.coders[created].thread, NULL, &coder_routine,
			&t.coders[created]))
		created++;
	release_start(&t, created < t.cli_args.values[NUM_CODERS]);
	i = -1;
	while (++i < created)
		pthread_join(t.coders[i].thread, NULL);
	pthread_join(monitor, NULL);
	destroy_table(&t);
	if (created < t.cli_args.values[NUM_CODERS])
		return (printf("Error: failed to create coder thread\n"), 1);
	return (0);
}
