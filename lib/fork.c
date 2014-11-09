// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void*) ROUNDDOWN(utf->utf_fault_va, PGSIZE);
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
    if (!(err & FEC_WR) || !(uvpt[PGNUM(addr)] & PTE_COW))
        panic("Fatal page fault");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
    if ((r = sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W)) < 0)
        panic("sys_page_alloc: %e", r);
    memmove(PFTEMP, addr, PGSIZE);
    if ((r = sys_page_map(0, PFTEMP, 0, addr, PTE_P | PTE_U | PTE_W)) < 0)
        panic("sys_page_map: %e", r);
    if ((r = sys_page_unmap(0, PFTEMP)) < 0)
        panic("sys_page_unmap: %e", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
    int perm = (PGOFF(uvpt[pn]) & PTE_SYSCALL);
    if (!(perm & PTE_SHARE) && (perm & PTE_W))
        perm = (perm & ~PTE_W) | PTE_COW;
    void *va = (void*) (pn * PGSIZE);
    int r;
    if ((r = sys_page_map(0, va, envid, va, perm)) < 0)
        panic("sys_page_map: %e", r);
    if ((r = sys_page_map(0, va, 0, va, perm)) < 0)
        panic("sys_page_map: %e", r);
    return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
    set_pgfault_handler(pgfault);
    envid_t envid = sys_exofork();
    if (envid < 0)
        return envid;
    if (envid == 0) {
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }

    // Copy page mappings
    int result;
    unsigned pn;
    for (pn = 0; pn < UTOP / PGSIZE; pn++)
        if (pn != UXSTACKTOP / PGSIZE - 1 &&
                (uvpd[pn / NPTENTRIES] & PTE_P) &&
                (uvpt[pn] & PTE_P) && (uvpt[pn] & PTE_U) &&
                (result = duppage(envid, pn)) < 0) {
            return result;
        }

    // Copy user exception stack page
    if ((result = sys_page_alloc(envid, (void*) (UXSTACKTOP - PGSIZE),
            PTE_P | PTE_U | PTE_W)) < 0)
        return result;
    if ((result = sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall)) < 0)
        return result;
    if ((result = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
        return result;

    return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
