#include "parser.h"
#include "codexion.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int	init_dongles(t_table *t, int n)
{
	int i;
	
	i = 0;
	memset(t->dongles, 0, sizeof(t_dongle) * n);
	while (i < n)
	{
		if (pthread_mutex_init(&t->dongles[i].lock, NULL))
			return (1);
		t->books.dongles_locks_ready++;
		if (pthread_cond_init(&t->dongles[i].cond, NULL))
			return (1);
		t->books.dongles_cond_ready++;
		t->dongles[i].queue[0].coder_id = -1;
		t->dongles[i].queue[1].coder_id = -1;
		t->dongles[i].id = i;
		i++;
	}
	return (0);
}

int init_coders(t_table *t, int n)
{
	// At the end of this function call, coder.thread is NULL
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
	// destroy the coder mutex, then free the memory;
	while (--t->books.coders_ready)
		pthread_mutex_destroy(&t->coders[t->books.coders_ready].lock);
	while (--t->books.dongles_locks_ready)
		pthread_mutex_destroy(&t->dongles[t->books.dongles_locks_ready].lock);
	while (--t->books.dongles_cond_ready)
		pthread_cond_destroy(&t->dongles[t->books.dongles_cond_ready].cond);
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

int	main(int argc, char *argv[])
{
	t_cliArgsValidation	args;
	t_cliArgs			cli_args;
	t_table				table;

	args = validate_cli_args(argc, argv);
	display_args(args);
	if (args.error)
		return (1);

	cli_args = args.cli_args;
	if (init_table(&table, cli_args))
		return (destroy_table(&table), 1);
	display_table(&table);
	return (0);
}
