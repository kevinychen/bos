#include <inc/lib.h>

void
umain(int argc, char **argv)
{
    binaryname = "pwd";
    printf("%s\n", thisenv->env_cwd);
}
