#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int i, r;

	binaryname = "rm";
    if (argc == 1)
        printf("error: missing operand\n");
    else for (i = 1; i < argc; i++)
        if ((r = remove(argv[i])) < 0)
            printf("error removing %s: %e\n", argv[i], r);
}

