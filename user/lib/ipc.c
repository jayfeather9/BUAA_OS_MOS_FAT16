// User-level IPC library routines

#include <env.h>
#include <lib.h>
#include <mmu.h>
#include <drivers/dev_rtc.h>

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

u_int get_time(u_int *us) {
	u_int temp = 1;
	syscall_write_dev(&temp, DEV_RTC_ADDRESS | DEV_RTC_TRIGGER_READ,
				sizeof(u_int));
	syscall_read_dev(&temp, DEV_RTC_ADDRESS | DEV_RTC_USEC,
				sizeof(u_int));
	*us = temp;
	syscall_read_dev(&temp, DEV_RTC_ADDRESS | DEV_RTC_SEC,
				sizeof(u_int));
	return temp;
}

void usleep(u_int us) {
	u_int e_ut;
	u_int e_t = get_time(&e_ut);
	u_int rem_t = us / 1000000;
	u_int rem_ut = us % 1000000;
	while (1) {
		u_int c_ut;
		u_int c_t = get_time(&c_ut);
		debugf("%u %u %u %u %u %u\n", e_t, e_ut, rem_t, rem_ut, c_t, c_ut);
		if (c_t > e_t + rem_t) {
			return;
		}
		else if (c_t >= e_t + rem_t && c_ut >= e_ut + rem_ut) {
			return;
		}
		else {
			syscall_yield();
		}
	}
}

