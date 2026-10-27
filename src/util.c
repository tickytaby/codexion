#include <stdlib.h>
#include <string.h>
#include "codexion.h"

int	init_dongles(t_table *t, int n)
{
	int	i;

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

int	init_coders(t_table *t, int n)
{
	int	i;

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
	int	res[2];

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
	t->dongles = malloc(sizeof(t_dongle) * args.values[NUM_CODERS]);
	if (!t->dongles || !t->coders)
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
