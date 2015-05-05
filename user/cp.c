#include <inc/lib.h>

char buf[63 * 64];

void
cp(int f1, int f2, char *s1, char *s2)
{
	long n;
	int r;

	while ((n = read(f1, buf, (long) sizeof(buf))) > 0)
		if ((r = write(f2, buf, n)) != n)
			panic("error writing %s: %e", s2, r);
	if (n < 0)
		panic("error reading %s: %e", s1, n);
}

void
umain(int argc, char **argv)
{
    int f1, f2;

    binaryname = "cp";
    if (argc != 3)
        printf("Usage: cp <src> <dest>\n");
    else {
        f1 = open(argv[1], O_RDONLY);
        if (f1 < 0) {
            printf("can't open %s: %e\n", argv[1], f1);
            return;
        }
        f2 = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC);
        if (f2 < 0) {
            printf("can't open %s: %e\n", argv[2], f2);
            return;
        }
        cp(f1, f2, argv[1], argv[2]);
        close(f1);
        close(f2);
    }
}
