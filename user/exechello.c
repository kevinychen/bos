#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int r;
	cprintf("i am parent environment %08x\n", thisenv->env_id);
	if ((r = execl("hello", "hello", 0)) < 0)
		panic("exec(hello) failed: %e", r);
    panic("current env still running\n");
}

