#ifndef CODEXION_H
# define CODEXION_H

# include <pthread.h>
# include "parser.h"

typedef struct Waiter
{
	int		coder_id;
	long	key;
}	t_waiter;

typedef struct Dongle
{
	int				id;
	pthread_mutex_t	lock;
	pthread_cond_t	cond;
	int				in_use;
	long			available_at;
	t_waiter		queue[2];
}	t_dongle;

typedef struct Coder
{
	int				id;
	pthread_t		thread;
	t_dongle		*left;
	t_dongle		*right;
	pthread_mutex_t	lock;
	long			last_compile_start;
	int				compiles_done;
	struct Table	*table;
}	t_coder;

typedef struct BookKeeping
{
	int	coders_ready;
	int	dongles_cond_ready;
	int	dongles_locks_ready;
	int	print_lock_ready;
	int	start_lock_ready;
	int	stop_lock_ready;
}	t_bookkeeping;

typedef struct Table
{
	t_cliArgs		cli_args;	
	long			start_time;
	int				start;
	int				stop;
	pthread_mutex_t	stop_lock;
	pthread_mutex_t	print_lock;
	pthread_mutex_t	start_lock;
	pthread_cond_t	start_cond;
	t_coder			*coders;
	t_dongle		*dongles;
	t_bookkeeping	books;
}	t_table;

long	now_ms(void);

#endif
