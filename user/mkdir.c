#include <inc/lib.h>

void
umain(int argc, char **argv)
{
    int f, i;

    binaryname = "mkdir";
    if (argc == 1)
        printf("Missing operand");
    else
        for (i = 1; i < argc; i++) {
            f = open(argv[i], O_CREAT | O_EXCL | O_MKDIR);
            if (f < 0)
                printf("can't mkdir %s: %e\n", argv[i], f);
            else
                close(f);
        }
}
