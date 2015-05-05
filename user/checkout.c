#include <inc/lib.h>

char buf[63 * 64];

void
checkout(char *s)
{
	long n;
	int f1, f2, r;
    char *p = s;

    while (*p && *p != '@')
        p++;
    if (*p != '@') {
        printf("error: require timestamp: %s\n", s);
        return;
    }

    f1 = open(s, O_RDONLY);
    if (f1 < 0)
        printf("can't open %s: %e\n", s, f1);
    *p = '\0';
    f2 = open(s, O_WRONLY | O_TRUNC);
    if (f2 < 0)
        printf("error writing %s: %e\n", s, f2);

    while ((n = read(f1, buf, (long) sizeof(buf))) > 0)
        if ((r = write(f2, buf, n)) != n)
            panic("error writing %s: %e", s, r);
	if (n < 0)
		panic("error reading %s: %e", s, n);

    close(f1);
    close(f2);
}

void
umain(int argc, char **argv)
{
    int f, i;
    char *p;

    binaryname = "checkout";
    for (i = 1; i < argc; i++)
        checkout(argv[i]);
}

