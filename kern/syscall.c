/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/e1000.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.
    user_mem_assert(curenv, s, len, 0);

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.
    struct Env *env_store;
    int result = env_alloc(&env_store, curenv->env_id);
    if (result < 0)
        return result;
    env_store->env_status = ENV_NOT_RUNNABLE;
    env_store->env_tf = curenv->env_tf;
    env_store->env_tf.tf_regs.reg_eax = 0;
    strcpy(env_store->env_cwd, curenv->env_cwd);
    return env_store->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
    struct Env *env_store;
    if (envid2env(envid, &env_store, 1) < 0)
        return -E_BAD_ENV;
    if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
        return -E_INVAL;
    env_store->env_status = status;
    return 0;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	struct Env *env_store;
    if (envid2env(envid, &env_store, 1) < 0)
        return -E_BAD_ENV;

    env_store->env_tf = *tf;
    env_store->env_tf.tf_cs |= 3;
    env_store->env_tf.tf_eflags |= FL_IF;
    return 0;
}

// Set all registers/data for curenv to those of envid.
// Frees all data associated with envid and destroys it.
//
// This function only returns on error. Errors are:
//  -E_BAD_ENV if environment envid doesn't currently exist,
//      or the caller doesn't have permission to change envid.
//  -E_INVAL if environment envid has already been run.
static int
sys_env_convert(envid_t envid)
{
    struct Env *env_store;
    if (envid2env(envid, &env_store, 1) < 0)
        return -E_BAD_ENV;

    if (env_store->env_runs > 0)
        return -E_INVAL;

    curenv->env_tf = env_store->env_tf;

    // Swap the page directories of curenv and env_id, so that
    // when we destroy env_id we free all of curenv's page tables.
    pde_t *temp_pgdir = curenv->env_pgdir;
    curenv->env_pgdir = env_store->env_pgdir;
    env_store->env_pgdir = temp_pgdir;

    env_destroy(env_store);
    sched_yield();
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
    struct Env *env_store;
    if (envid2env(envid, &env_store, 1) < 0)
        return -E_BAD_ENV;
    env_store->env_pgfault_upcall = func;
    return 0;
}

static int
validate_va(void *va)
{
    return (uintptr_t) va < UTOP && ROUNDDOWN(va, PGSIZE) == va;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
    struct Env *env_store;
    if (envid2env(envid, &env_store, 1) < 0)
        return -E_BAD_ENV;
    if (!validate_va(va))
        return -E_INVAL;
    if ((perm ^ (PTE_U | PTE_P)) & ~(PTE_AVAIL | PTE_W))
        return -E_INVAL;

    struct PageInfo *page = page_alloc(ALLOC_ZERO);
    if (page_insert(env_store->env_pgdir, page, va, perm) < 0) {
        page_free(page);
        return -E_NO_MEM;
    }

    return 0;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
    struct Env *srcenv, *dstenv;
    if (envid2env(srcenvid, &srcenv, 1) < 0)
        return -E_BAD_ENV;
    if (envid2env(dstenvid, &dstenv, 1) < 0)
        return -E_BAD_ENV;
    if (!validate_va(srcva))
        return -E_INVAL;
    if (!validate_va(dstva))
        return -E_INVAL;

    pte_t *pte;
    struct PageInfo *srcpage = page_lookup(srcenv->env_pgdir, srcva, &pte);
    if (!srcpage)
        return -E_INVAL;
    if ((perm ^ (PTE_U | PTE_P)) & ~(PTE_AVAIL | PTE_W))
        return -E_INVAL;
    if ((perm & PTE_W) && !(*pte & PTE_W))
        return -E_INVAL;

    return page_insert(dstenv->env_pgdir, srcpage, dstva, perm);
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
    struct Env *env_store;
    if (envid2env(envid, &env_store, 1) < 0)
        return -E_BAD_ENV;
    if (!validate_va(va))
        return -E_INVAL;

    page_remove(env_store->env_pgdir, va);
    return 0;
}

// Set the current working directory of the current environment.
static int
sys_chdir(const char *path)
{
    strcpy(curenv->env_cwd, path);
    return 0;
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
    struct Env *dstenv;
    if (envid2env(envid, &dstenv, 0) < 0)
        return -E_BAD_ENV;
    if (!dstenv->env_ipc_recving)
        return -E_IPC_NOT_RECV;
    if (dstenv->env_ipc_srcenv && dstenv->env_ipc_srcenv != curenv->env_id)
        return -E_IPC_NOT_RECV;

    bool pageTransferred = false;
    if (srcva < (void*) UTOP) {
        if (ROUNDDOWN(srcva, PGSIZE) != srcva)
            return -E_INVAL;
        if ((perm ^ (PTE_U | PTE_P)) & ~(PTE_AVAIL | PTE_W))
            return -E_INVAL;

        int checkPerm = PTE_U | PTE_P;
        if (perm & PTE_W)
            checkPerm |= PTE_W;
        if (user_mem_check(curenv, srcva, PGSIZE, checkPerm) < 0)
            return -E_INVAL;

        if (dstenv->env_ipc_dstva < (void*) UTOP) {
            struct PageInfo *srcpage = page_lookup(
                    curenv->env_pgdir, srcva, 0);
            if (page_insert(dstenv->env_pgdir, srcpage,
                    dstenv->env_ipc_dstva, perm) < 0) {
                return -E_NO_MEM;
            }
            pageTransferred = true;
        }
    }

    dstenv->env_ipc_recving = false;
    dstenv->env_ipc_from = curenv->env_id;
    dstenv->env_ipc_value = value;
    dstenv->env_ipc_perm = pageTransferred ? perm : 0;
    dstenv->env_status = ENV_RUNNABLE;
    dstenv->env_tf.tf_regs.reg_eax = 0;  // return 0 in dstenv
    return 0;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
// If srcenv is nonzero, then you will only receive from that environment.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva, envid_t srcenv)
{
    if (dstva < (void*) UTOP) {
        if (ROUNDDOWN(dstva, PGSIZE) != dstva)
            return -E_INVAL;

        curenv->env_ipc_dstva = dstva;
    }
    curenv->env_ipc_recving = true;
    curenv->env_status = ENV_NOT_RUNNABLE;
    curenv->env_ipc_srcenv = srcenv;
    sched_yield();
}

// Return the current time.
static int
sys_time_msec(void)
{
    return time_msec();
}

// Transmit a packet.
// Returns 0 on success, < 0 on error.
// Errors are:
//  -E_INVAL if len is too large.
static int
sys_net_transmit(void *va, uint32_t len)
{
    user_mem_assert(curenv, va, len, 0);
    if (len > MAX_PACKET_LEN)
        return -E_INVAL;

    pte_t *pte;
    page_lookup(curenv->env_pgdir, va, &pte);
    e1000_transmit(PTE_ADDR(*pte) | PGOFF(va), (uint16_t) len);
    return 0;
}

// Receive a packet.
// Returns length of packet on success, < 0 on error.
static int
sys_net_receive(void *va)
{
    user_mem_assert(curenv, va, MAX_PACKET_BUF, 0);

    pte_t *pte;
    page_lookup(curenv->env_pgdir, va, &pte);
    return e1000_receive(PTE_ADDR(*pte) | PGOFF(va));
}

// Return the low bits of the MAC address
static int
sys_mac_addr_low()
{
    return e1000_mac_addr_low();
}

// Return the high bits of the MAC address
static int
sys_mac_addr_high()
{
    return e1000_mac_addr_high();
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	switch (syscallno) {
        case SYS_cputs:
            sys_cputs((const char*) a1, (size_t) a2);
            return 0;
        case SYS_cgetc:
            return sys_cgetc();
        case SYS_getenvid:
            return sys_getenvid();
        case SYS_env_destroy:
            return sys_env_destroy((envid_t) a1);
        case SYS_page_alloc:
            return sys_page_alloc((envid_t) a1, (void*) a2, (int) a3);
        case SYS_page_map:
            return sys_page_map((envid_t) a1, (void*) a2,
                    (envid_t) a3, (void*) a4, (int) a5);
        case SYS_page_unmap:
            return sys_page_unmap((envid_t) a1, (void*) a2);
        case SYS_chdir:
            return sys_chdir((const char*) a1);
        case SYS_exofork:
            return sys_exofork();
        case SYS_env_set_status:
            return sys_env_set_status((envid_t) a1, (int) a2);
        case SYS_env_set_trapframe:
            return sys_env_set_trapframe((envid_t) a1, (struct Trapframe*) a2);
        case SYS_env_convert:
            return sys_env_convert((envid_t) a1);
        case SYS_env_set_pgfault_upcall:
            return sys_env_set_pgfault_upcall((envid_t) a1, (void*) a2);
        case SYS_yield:
            sys_yield();  // does not return
        case SYS_ipc_try_send:
            return sys_ipc_try_send((envid_t) a1, (uint32_t) a2,
                    (void*) a3, (unsigned) a4);
        case SYS_ipc_recv:
            sys_ipc_recv((void*) a1, (envid_t) a2);  // does not return
        case SYS_time_msec:
            return sys_time_msec();
        case SYS_net_transmit:
            return sys_net_transmit((void*) a1, (uint32_t) a2);
        case SYS_net_receive:
            return sys_net_receive((void*) a1);
        case SYS_mac_addr_low:
            return sys_mac_addr_low();
        case SYS_mac_addr_high:
            return sys_mac_addr_high();
        default:
            return -E_INVAL;
	}
}

