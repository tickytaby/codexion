#include "parser.h"
#include "codexion.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int	init_dongles(t_table *t, int n)
{
	int i;
	
	i = 0;
	memset(&t->dongles, 0, sizeof(t_dongle) * n);
	while (i < n)
	{
		if (pthread_mutex_init(&t->dongles[i].lock, NULL))
			return (1);
		t->dongles_locks_ready++;
		if (pthread_cond_init(&t->dongles[i].cond, NULL))
			return (1);
		t->dongles_cond_ready++;
		t->dongles[i].queue[0].coder_id = -1;
		t->dongles[i].queue[1].coder_id = -1;
		i++;
	}
	return (0);
}

int init_coders(t_table *t, int n)
{
	// At the end of this function call, coder.thread is NULL
	int i;

	i = 0;
	memset(&t->coders, 0, sizeof(t_coder) * n);
	while (i < n)
	{
		if (pthread_mutex_init(&t->coders[i].lock, NULL))
			return (1);
		t->coders[i].id = i;
		t->coders[i].left = &t->dongles[i];
		t->coders[i].right = &t->dongles[(i + 1) % n];
		t->coders[i].table = t;
	}
	return (0);
}

int	init_table(t_table *t, t_cliArgs args)
{
	// Need to initialize stop_lock, print_lock, start_lock
	if (pthread_mutex_init(&t->print_lock, NULL))
		return (1);
	if (pthread_mutex_init(&t->start_lock, NULL))
		return (1);
	if (pthread_mutex_init(&t->stop_lock, NULL))
		return (1);
	memset(t, 0, sizeof(t_table));
	t->cli_args = args;
	init_coders(t, args.values[NUM_CODERS]);
	init_dongles(t, args.values[NUM_CODERS]);
	return (0);
}

// NEED TO WRITE THE CLEAN UP FUNCTION THAT TAKES CARE OF DONGLES, CODERS, AND TABLE ITSELF

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
	init_table(&table, cli_args);

	return (0);
}
