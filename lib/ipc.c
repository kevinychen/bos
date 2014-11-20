// User-level IPC library routines

#include <inc/lib.h>

// Receive a value via IPC and return it.
// If 'pg' is nonnull, then any page sent by the sender will be mapped at
//	that address.
// If 'from_env_store' is nonnull, then store the IPC sender's envid in
//	*from_env_store.
// If 'perm_store' is nonnull, then store the IPC sender's page permission
//	in *perm_store (this is nonzero iff a page was successfully
//	transferred to 'pg').
// If the system call fails, then store 0 in *fromenv and *perm (if
//	they're nonnull) and return the error.
// Otherwise, return the value sent by the sender
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store, envid_t srcenv)
{
    if (pg == NULL)
        pg = (void*) UTOP;  // an invalid address

    int result = sys_ipc_recv(pg, srcenv);
    if (result < 0) {
        if (from_env_store)
            *from_env_store = 0;
        if (perm_store)
            *perm_store = 0;
        return result;
    } else {
        if (from_env_store)
            *from_env_store = thisenv->env_ipc_from;
        if (perm_store)
            *perm_store = thisenv->env_ipc_perm;
        return thisenv->env_ipc_value;
    }
}

// Send 'val' (and 'pg' with 'perm', if 'pg' is nonnull) to 'toenv'.
// This function keeps trying until it succeeds.
// It should panic() on any error other than -E_IPC_NOT_RECV.
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
    if (pg == NULL)
        pg = (void*) UTOP;  // an invalid address

    int result;
    while ((result = sys_ipc_try_send(to_env, val, pg, perm)) ==
            -E_IPC_NOT_RECV) {
        sys_yield();
    }
    if (result < 0)
        panic("ipc_send: %e\n", result);
}

// Find the first environment of the given type.  We'll use this to
// find special environments.
// Returns 0 if no such environment exists.
envid_t
ipc_find_env(enum EnvType type)
{
	int i;
	for (i = 0; i < NENV; i++)
		if (envs[i].env_type == type)
			return envs[i].env_id;
	return 0;
}
