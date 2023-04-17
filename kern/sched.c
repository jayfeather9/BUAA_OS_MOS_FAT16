#include <env.h>
#include <pmap.h>
#include <printk.h>

/* Overview:
 *   Implement a round-robin scheduling to select a runnable env and schedule it using 'env_run'.
 *
 * Post-Condition:
 *   If 'yield' is set (non-zero), 'curenv' should not be scheduled again unless it is the only
 *   runnable env.
 *
 * Hints:
 *   1. The variable 'count' used for counting slices should be defined as 'static'.
 *   2. Use variable 'env_sched_list', which contains and only contains all runnable envs.
 *   3. You shouldn't use any 'return' statement because this function is 'noreturn'.
 */
void schedule(int yield) {
	static int count = 0; // remaining time slices of current env
	struct Env *e = curenv;

	static int user_time[5];

	/* We always decrease the 'count' by 1.
	 *
	 * If 'yield' is set, or 'count' has been decreased to 0, or 'e' (previous 'curenv') is
	 * 'NULL', or 'e' is not runnable, then we pick up a new env from 'env_sched_list' (list of
	 * all runnable envs), set 'count' to its priority, and schedule it with 'env_run'. **Panic
	 * if that list is empty**.
	 *
	 * (Note that if 'e' is still a runnable env, we should move it to the tail of
	 * 'env_sched_list' before picking up another env from its head, or we will schedule the
	 * head env repeatedly.)
	 *
	 * Otherwise, we simply schedule 'e' again.
	 *
	 * You may want to use macros below:
	 *   'TAILQ_FIRST', 'TAILQ_REMOVE', 'TAILQ_INSERT_TAIL'
	 */
	/* Exercise 3.12: Your code here. */

	struct Env *avail_es[5];
	for (int i = 0; i < 5; i++) avail_es[i] = NULL;
	struct Env *ie;
	TAILQ_FOREACH(ie, &env_sched_list, env_sched_link) {
		int user = ie->env_user;
		if (avail_es[user] == NULL) {
			avail_es[user] = ie;
		}
	}

	// four situations we pick up a new env
	if (yield || (count <= 0) || (e == NULL) || (e->env_status != ENV_RUNNABLE)) {
		// remove e
		if (e != NULL) {
			TAILQ_REMOVE(&env_sched_list, e, env_sched_link);
			// if e is still runnable, insert it
			if (e->env_status == ENV_RUNNABLE) {
				TAILQ_INSERT_TAIL(&env_sched_list, e, env_sched_link);
				user_time[e->env_user] += e->env_pri;
			}
		}
		
		// pick up a new env
		if (TAILQ_EMPTY(&env_sched_list)) panic("no runnable envs");
		// e = TAILQ_FIRST(&env_sched_list);

		int min_user_time = 2100000000;
		int min_user = -1;
		for (int usr_id = 0; usr_id < 5; usr_id++) {
			if (avail_es[usr_id] == NULL) continue;
			if (user_time[usr_id] < min_user_time) {
				min_user_time = user_time[usr_id];
				min_user = usr_id;
			}
		}

		if (min_user == -1) panic("cannot find min user");
		e = avail_es[min_user];

		// set count to priority
		count = e->env_pri;
	}

	// "We always decrease the 'count' by 1."
	count--;

	// schedule e again (if we should pick up a new env, e have been reassigned)
	env_run(e);
}
