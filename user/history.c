#include <inc/lib.h>

time_t buf[PGSIZE / sizeof(time_t)];

void
list_history(int f, char *s)
{
    long n;
    int r, i;

    n = history(f, buf, PGSIZE / sizeof(time_t) / 8, 0);
    if (n < 0)
        panic("error getting history of %s: %e", s, n);
    for (i = 0; i < n; i++)
        printf("%s@%lld\n", s, buf[i]);
}

void
umain(int argc, char **argv)
{
	int f, i;

	binaryname = "history";
	for (i = 1; i < argc; i++) {
		f = open(argv[i], O_RDONLY);
		if (f < 0)
			printf("can't open %s: %e\n", argv[i], f);
		else {
			list_history(f, argv[i]);
			close(f);
		}
	}
}

