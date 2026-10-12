#ifndef UTIL_H
# define UTIL_H

# include "codexion.h"

int				init_dongles(t_table *t, int n);
int				init_coders(t_table *t, int n);
int				init_table(t_table *t, t_cliArgs args);
void			destroy_table(t_table *t);
void			display_table(t_table *t);
long			queue_key(t_coder *c);
void			push(t_coder *c, t_dongle *d, long key);
void			heapify(t_waiter *q);
void			enq(t_coder *c, t_dongle *d, long key);
void			deq(t_dongle *d, int coder_id);
long			now_ms(void);
t_waiter		*peek(t_waiter *q);
int				stopped(t_table *t);
int				sim_sleep(t_table *t, long ms);
t_dongle		*other_dongle(t_coder *c, t_dongle *d);
int				can_take(t_coder *c, t_dongle *d);
void			log_action(t_coder *c, char *action);
void			wait_dongles(t_coder *c);
void			lock_and_enq(t_table *t, t_coder *c, long key);
void			deq_and_unlock(t_table *t, t_coder *c);
int				take_dongles(t_coder *c);
void			release_dongles(t_coder *c);
int				compile(t_coder *c);
void			refactor(t_coder *c);
void			debug(t_coder *c);
struct timespec	ms_to_ts(long ms);
#endif
