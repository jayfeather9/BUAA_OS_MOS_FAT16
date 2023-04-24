// User-level IPC library routines

#include <env.h>
#include <lib.h>
#include <mmu.h>

// Send val to whom.  This function keeps trying until
// it succeeds.  It should panic() on any error other than
// -E_IPC_NOT_RECV.
//
// Hint: use syscall_yield() to be CPU-friendly.
void ipc_send(u_int whom, u_int val, const void *srcva, u_int perm) {
	int r;
	while ((r = syscall_ipc_try_send(whom, val, srcva, perm)) == -E_IPC_NOT_RECV) {
		syscall_yield();
	}
	user_assert(r == 0);
}

// Receive a value.  Return the value and store the caller's envid
// in *whom.
//
// Hint: use env to discover the value and who sent it.
u_int ipc_recv(u_int *whom, void *dstva, u_int *perm) {
	int r = syscall_ipc_recv(dstva);
	if (r != 0) {
		user_panic("syscall_ipc_recv err: %d", r);
	}

	if (whom) {
		*whom = env->env_ipc_from;
	}

	if (perm) {
		*perm = env->env_ipc_perm;
	}

	return env->env_ipc_value;
}

u_int in_family[NENV];

void ipc_broadcast(u_int val, void * srcva, u_int perm) {
	debugf("going in func\n");
	u_int curid = syscall_getenvid();

	for (int i = 0; i < NENV; i++) in_family[i] = 0;
	in_family[curid] = 1;

	debugf("init completed!\n");

	int can_end;
BEGIN_SEARCH:
	can_end = 1;
	debugf("doing one search\n");
	for (int i = 0; i < NENV; i++) {
		struct Env e = envs[i];
		if (in_family[e.env_parent_id] && !in_family[e.env_id]) {
			in_family[e.env_id] = 1;
			can_end = 0;
		}
		// debugf("1");
	}
	if (!can_end) goto BEGIN_SEARCH;

	debugf("search completed!\n");
		
	for (int i = 0; i < NENV; i++) {
		struct Env e = envs[i];
		if (e.env_status == ENV_FREE) continue;
		if (e.env_id == curid) continue;
		if (!in_family[e.env_id]) continue;

		int r;
		while ((r = syscall_ipc_try_send(e.env_id, val, srcva, perm)) ==
				-E_IPC_NOT_RECV);
	}
	syscall_yield();
}
