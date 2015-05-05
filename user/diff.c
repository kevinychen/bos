#include <inc/lib.h>
#include <inc/string.h>

#define BUF_SIZE 63 * 64

char buf1[BUF_SIZE + 1];
char buf2[BUF_SIZE + 1];
char comp[BUF_SIZE];

void
diff(int f1, int f2, char *s1, char *s2)
{
    long n1, n2;
    int i, r;

    while (1) {
        n1 = read(f1, buf1, BUF_SIZE);
        if (n1 < 0)
            panic("error reading %s: %e", s1, n1);
        n2 = read(f2, buf2, BUF_SIZE);
        if (n2 < 0)
            panic("error reading %s: %e", s2, n2);
        if (n1 + n2 == 0)
            break;

        for (i = 0; i < n1 || i < n2; i++)
            comp[i] = ' ';
        for (i = 0; i < n1 && i < n2; i++)
            if (buf1[i] && buf2[i] && buf1[i] == buf2[i])
                comp[i] = '|';
        comp[i++] = '\n';

        for (r = 0; r < n1; r++)
            if (buf1[r] < 32)
                buf1[r] = ' ';
        buf1[n1] = '\n';

        for (r = 0; r < n2; r++)
            if (buf2[r] < 32)
                buf2[r] = ' ';
        buf2[n2] = '\n';

        if ((r = write(1, buf1, n1 + 1)) != n1 + 1 ||
                (r = write(1, comp, i)) != i ||
                (r = write(1, buf2, n2 + 1)) != n2 + 1)
            panic("error writing: %e\n", r);
    }
}

void
umain(int argc, char **argv)
{
    int f1, f2;

    binaryname = "diff";
    if (argc != 3)
        printf("Usage: diff <file1> <file2>\n");
    else {
        f1 = open(argv[1], O_RDONLY);
        if (f1 < 0) {
            printf("can't open %s: %e\n", argv[1], f1);
            return;
        }
        f2 = open(argv[2], O_RDONLY);
        if (f2 < 0) {
            printf("can't open %s: %e\n", argv[2], f2);
            return;
        }
        diff(f1, f2, argv[1], argv[2]);
        close(f1);
        close(f2);
    }
}
