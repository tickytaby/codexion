*This project has been created as part of the 42 curriculum by ryin.*

# Codexion

## Description

Codexion is a concurrency project in C. A group of coders sits around a table in a co-working hub, and between every pair of neighbours there is one USB dongle. To compile, a coder needs both the dongle on their left and the one on their right at the same time. After compiling they put the dongles back, debug, refactor, and then go back for the dongles again. If a coder goes `time_to_burnout` ms without *starting* a compile, they burn out and the simulation ends.

On top of the classic "dining philosophers" setup there are two extra rules that make it harder:

- **Dongle cooldown**: once a dongle is put down it can't be picked up again until `dongle_cooldown` ms have passed.
- **Fair arbitration**: when two coders want the same dongle, the dongle decides who gets it, either `fifo` (who asked first) or `edf` (earliest deadline first, deadline = `last_compile_start + time_to_burnout`).

Every coder is a pthread, a separate monitor thread watches for burnout, and all the output goes through a single mutex so lines never get mixed up.

## Instructions

### Build

```sh
make        # builds ./codexion
make clean  # removes object files
make fclean # removes object files and the binary
make re     # full rebuild
```

The project compiles with `cc -Wall -Wextra -Werror -pthread`. There are also two rules I used while debugging:

```sh
make debug  # -g -fsanitize=address,undefined
make tsan   # -g -fsanitize=thread
```

### Run

```sh
./codexion number_of_coders time_to_burnout time_to_compile time_to_debug \
           time_to_refactor number_of_compiles_required dongle_cooldown scheduler
```

All 8 arguments are mandatory. Times are in milliseconds, and `scheduler` must be exactly `fifo` or `edf`.

```sh
./codexion 4 610 200 100 100 5 0 edf     # nobody should burn out
./codexion 5 800 200 200 200 7 10 fifo   # nobody should burn out
./codexion 4 300 200 100 100 5 0 edf     # someone burns out at ~300
./codexion 1 400 100 100 100 3 0 edf     # one coder, one dongle -> burns out at 400
```

Output looks like this:

```
0 2 has taken a dongle
0 2 has taken a dongle
0 2 is compiling
200 2 is debugging
...
```

### Input validation

The parser rejects:

- the wrong number of arguments
- anything that isn't made only of digits (so `-5`, `+5`, `8.5`, `abc`)
- values bigger than `INT_MAX`. I build the number digit by digit and stop as soon as it goes past `INT_MAX`, instead of calling `atoi`, which silently wraps (`4294967396` used to become a 100 ms burnout). Leading zeros are fine: `0000000042` is 42.
- `0` coders
- any scheduler other than `fifo` / `edf`

Errors go to `stderr` and the exit code is `1`.

If `number_of_compiles_required` is `0`, the stop condition is already true before anyone has done anything, so the simulation never starts: nothing is allocated, no threads are created, and the program exits with `0`.

## How it works

### Overall structure

```
main
 ├─ validate args
 ├─ init_table      (mutexes, conds, coders, dongles)
 ├─ create monitor thread
 ├─ create N coder threads   ── all of them block on start_cond
 ├─ release_start   (sets start_time + every last_compile_start, broadcasts)
 ├─ join everything
 └─ destroy_table
```

Every thread waits at the start barrier (`start_lock` + `start_cond`) until all of them have been created. That way coder 1 doesn't get a head start of a few ms over coder 200 just because its thread was created first, and everyone's burnout clock starts from the same `start_time`.

The coder loop is basically:

```c
while (!stopped(table))
{
    if (take_dongles(c))   // returns 1 if the simulation stopped while waiting
        break ;
    compile(c);
    release_dongles(c);
    debug(c);
    refactor(c);
}
```

### The dongle queues (my "heap")

Each dongle has a priority queue of waiting coders, ordered by a key:

- `fifo`: key = the time the request was made
- `edf`: key = `last_compile_start + time_to_burnout`

If two keys are equal, a global, increasing `ticket` breaks the tie, so the order is always a strict total order and the result is deterministic (the subject explicitly asks for a tie-breaker).

The queue is a `t_waiter queue[2]` with a compare-and-swap `heapify`. I know this looks like it's dodging the "implement a heap" requirement, but it isn't: a dongle sits between exactly two coders, so its queue can never hold more than two entries. A binary heap with two elements is just "the smaller one goes on top", and that's exactly what `heapify` does. Writing generic sift-up/sift-down for an array that can never grow past 2 would add code that does nothing different.

A coder is only in a queue while it's *waiting*. As soon as it gets its dongles it removes itself from both queues, so the head of a queue is always the highest-priority coder that still wants that dongle.

## Evolution of the dongle logic

This part took me the most rewrites, so I'm going to explain how it got to where it is.

### Version 1: one mutex per dongle, one dongle at a time

My first version was the textbook approach. Every dongle had its own `pthread_mutex_t`, its own `pthread_cond_t` and its own queue. A coder would:

1. lock the first dongle (always the one with the higher id, so the lock order is global and you can't get a circular wait),
2. queue on it, wait until it's first and the dongle is free and cooled down, take it, log `has taken a dongle`,
3. unlock it, then do the same thing for the second dongle.

This doesn't deadlock: ordering the locks breaks Coffman's circular-wait condition. But it still has **hold and wait**: a coder holds one dongle while it waits for the other. In the normal philosophers problem that's fine, because all you care about is that nobody deadlocks. Here it causes two problems:

- **The dongle you're holding is wasted.** If coder A holds dongle X and waits for Y, X is in use but nobody is compiling with it. Coder B, on the other side of X, might be able to compile right now and might be much closer to burning out, but X is "taken", so B waits too. Chains like this build up around the table, and with cooldown on top each wasted slot gets longer. With feasible parameters I was still getting burnouts.
- **The scheduler can't really be fair.** Each dongle ran its own queue on its own, so a coder could be first on one dongle and last on the other. EDF is supposed to serve whoever is closest to burning out, but the coder holding one dongle wasn't giving it up for anybody, regardless of deadlines.

Taking the dongles one at a time also meant the two `has taken a dongle` lines for a compile could be far apart in the log, with a dongle held the whole time in between.

### Version 2: take both dongles at once

The fix was to stop holding one dongle while waiting for the other. A coder now takes **both dongles in one step, or neither**:

- There's now a single `dongle_lock` and a single `dongle_cond` for the whole table instead of one per dongle. This was the main tradeoff. In theory one global lock means more contention, but the critical sections are tiny (check two flags, compare two queue heads), and it's the only clean way to look at both dongles *and* both queues at the same moment. With per-dongle locks I'd have had to lock two mutexes in order again just to read the state, which brings back the problem I was trying to remove.
- `take_dongles` queues the coder on **both** dongles with the same key, then waits until `can_take` is true for both dongles at once. Only then does it mark both `in_use`, leave both queues and log the two `has taken a dongle` lines back to back.
- Since nobody ever holds a dongle while waiting, hold-and-wait is gone, and so is any chance of a circular wait.

### Version 2.5: the head of the queue was blocking everyone

Version 2 originally had a rule that let a coder go ahead of the head of a dongle's queue if that head wasn't also first in the queue for its *other* dongle (the idea being "if they can't get both anyway, don't let them block this one"). The problem is that the rule only looked at queue positions and never at whether the dongles were actually free.

Here's a real run that failed with `4 610 200 100 100 5 0 edf`:

```
0 2 has taken a dongle
0 2 has taken a dongle
0 2 is compiling
200 1 has taken a dongle      <- coder 4 could have been compiling since 0
...
400 4 is compiling
...
600 3 is compiling
610 2 burned out
```

At `t = 0` coder 2 has dongles 1 and 2. Coder 4 wants dongles 3 and 0, and both are free. But coder 3 is at the head of dongle 3's queue, and coder 3 is also first on dongle 2, so the rule says "coder 3 is about to get going, don't skip them". Except coder 3 can't get going, because dongle 2 is being used by coder 2. So coder 4 just sits there, everyone ends up compiling **one at a time**, and it takes 4 × 200 ms to get around the table. That's well past 610.

You could argue that's just the scheduler doing its job, since the head has priority. But at `t = 0` everyone has the same deadline (`start + 610`), so coder 3 only won on the arrival ticket, not because it was more urgent. And forcing a dongle to stay free for someone who can't use it yet doesn't help that person at all. It just makes everybody else wait.

### Version 3 (current): only skip ahead if it doesn't delay the head

The rule now is: a coder `c` can take dongle `d` ahead of the head `h` of `d`'s queue **only if `c` will be done with `d` before `h` could have used it anyway**:

```
now + time_to_compile + dongle_cooldown  <=  available_at of h's other dongle
```

To make this work, `available_at` now means "when this dongle is free again" all the time, not only after it's released. When a coder takes its dongles, `deq_and_unlock` sets both to `now + time_to_compile + dongle_cooldown` (the earliest they can come back), and `release_dongles` overwrites that with the real time when they're actually put down. Nothing else gets confused by the estimate: `can_take` checks `in_use` before it looks at `available_at`, and `wait_dongles` only looks at `available_at` when both dongles are already free.

In the run above: coder 2 holds dongle 2 until 200, coder 4 would finish with dongle 3 at `0 + 200 = 200`, and `200 <= 200`, so coder 4 goes. Coder 3 still starts at 200, exactly as it would have, but now two coders are compiling at the same time instead of one.

So EDF/FIFO order is still respected for every coder who can actually start, and a dongle only goes to someone else during time when the head couldn't have used it. Schedulers call this **backfilling**.

## Blocking cases handled

**Deadlock (Coffman's conditions).** A deadlock needs all four of mutual exclusion, hold and wait, no preemption, and circular wait. Mutual exclusion and no preemption are just how dongles work here. I remove **hold and wait** because a coder takes both dongles at once or none of them, and without hold and wait there's no **circular wait** either. Inside the program, the only times two mutexes are held at once are `dongle_lock` → `stop_lock` (through `stopped()`), `print_lock` → `stop_lock` (in `log_action`) and `start_lock` → `coder.lock` (in `release_start`). They're always taken in that order and none of them goes the other way, so there's no lock-ordering cycle between mutexes either.

**Starvation.** Every waiting coder is in a priority queue on both of its dongles. Under EDF, the longer you wait the closer your deadline gets compared to the neighbours who just compiled, so you rise to the top. The only way to get ahead of the head of a queue is the backfill rule, which by design never delays the head.

**Dongle cooldown.** Every dongle stores `available_at`, and a dongle can only be taken if `!in_use && available_at <= now`. When a coder's two dongles are both free but at least one is still cooling down, it doesn't busy-wait. It does a `pthread_cond_timedwait` until the later of the two cooldowns ends. Otherwise it does a plain `pthread_cond_wait` until somebody releases something.

**Precise burnout detection.** The monitor thread loops every 1 ms, and for each coder it locks that coder's mutex and checks `now >= last_compile_start + time_to_burnout`. When someone burns out, it sets `stop`, broadcasts on `dongle_cond` so nobody stays asleep waiting for a dongle, and prints the `burned out` line. In my tests the line comes out 0–1 ms after the actual deadline, well inside the 10 ms limit. Sleeps (`sim_sleep`) are done in short chunks that check `stop`, so coders notice the end quickly too.

**Log serialization.** All logging goes through `log_action`, which takes `print_lock`, computes the timestamp, checks `stopped()` and only then calls `printf`. Because `stop` is checked *inside* the print lock, once the monitor has set `stop` no coder can print anything else, so `burned out` is always the last line.

**One coder.** With a single coder there's only one dongle (`left == right`). The coder takes it, logs it, and then just waits until the monitor stops the simulation, because it can never compile.

**Thread creation failures.** If `pthread_create` fails halfway through, `release_start` sets `stop` before opening the start barrier, so the threads that were already created wake up, see `stop`, and exit. Then everything gets joined and freed. `init_table` keeps track of which mutexes/conds were actually initialized (`t_bookkeeping`), so `destroy_table` only destroys those, even if initialization failed partway.

## Thread synchronization mechanisms

| Primitive | Protects | Used by |
|---|---|---|
| `dongle_lock` (`pthread_mutex_t`) | every dongle's `in_use`, `available_at` and queue, plus the global `ticket` counter | coders (take/release), monitor (stop broadcast) |
| `dongle_cond` (`pthread_cond_t`) | "something about the dongles changed" | coders wait on it; release, take, enqueue and stop broadcast on it |
| `coder.lock` (one per coder) | `last_compile_start`, `compiles_done` | the coder writes them, the monitor reads them |
| `stop_lock` | the `stop` flag | written only by the monitor (and by `main` if thread creation fails), read by everyone through `stopped()` |
| `print_lock` | stdout | `log_action`, and the monitor's burnout line |
| `start_lock` + `start_cond` | the `start` flag and the start barrier | `main` opens it, all threads wait on it |

Some examples of races these prevent:

- **Two coders grabbing the same dongle.** The check (`can_take` on both dongles) and the take (`in_use = 1`) happen inside the same `dongle_lock` critical section, so no other coder can get in between "it's free" and "it's mine".
- **The monitor reading half-updated coder state.** `compile()` updates `last_compile_start` under the coder's own lock, and the monitor reads it under the same lock. The monitor never sees a value that's only partly written, and it never misses a compile that started just before it checked.
- **Lost wakeups.** Coders only call `pthread_cond_wait` while holding `dongle_lock`, and they always re-check the condition in a `while` loop. Every change that could unblock someone (release, take, a new request, stop) broadcasts while holding the lock, so a wakeup can't slip through between the check and the wait.
- **Coders printing after a burnout.** As explained above, `stop` is checked inside `print_lock`.
- **`start_time` being read too early.** It's written in `release_start` under `start_lock` before the broadcast, and every thread reads it only after it has passed the barrier under that same lock, so they all see the final value.

Communication between the coders and the monitor goes one way in each direction: coders publish their state through their own `lock`, and the monitor publishes the end of the simulation through `stop_lock` plus a broadcast on `dongle_cond`. Coders never talk to each other directly. The only things they share are the dongles and the queues, which is how the subject describes it ("coders do not communicate with each other").

I checked the result with valgrind (no leaks, no errors) and helgrind (no data races or lock-order warnings).

## Known limitations

- With very large tables (around 200 coders) and tight timings, I still get an occasional burnout even when the numbers are feasible on paper. My guess is that with that many threads, scheduling jitter (`usleep` overshooting, threads waking up late) adds up faster than the time the backfill check leaves for it. For normal sizes, I haven't seen a burnout with feasible parameters, with either scheduler.
- When the backfill rule lets someone skip ahead, it assumes compiles take exactly `time_to_compile`. If a compile runs ~1 ms long because of OS scheduling, the head can be delayed by about that much.

## Resources

- `man 3 pthread_mutex_lock`, `pthread_cond_wait`, `pthread_cond_timedwait`, `gettimeofday`

### How I used AI

I used Claude (Anthropic) as a reviewer, not to write the simulation:

- I had it go through the subject and my code to list the requirements I hadn't covered yet (missing README, missing 42 headers, integer overflow in the parser, misleading error messages, errors on stdout instead of stderr, leftover debug functions).
- It ran stress tests, found the "one coder at a time" problem from version 2.5 and traced it back to `can_take`. I pushed back first (my argument was "the head is supposed to have priority"), and in that discussion it proposed the backfill condition as a way to keep the head's priority without leaving dongles sitting unused. I researched about it and later implemented it.
- It wrote a first draft of this README from our design discussions, and I edited it so it matches the code and the way I'd explain it.
