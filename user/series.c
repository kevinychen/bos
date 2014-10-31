// Based on "Squinting at Power Series" by Doug McIlroy.
// See http://plan9.bell-labs.com/who/rsc/thread/squint.pdf.
//
// This program calculates sin(F), where F = x + x^3.

#include <inc/lib.h>

/* Fraction library functions */

typedef struct {
    int num;
    int den;
} Frac;

static int
gcd(int a, int b)
{
    return b == 0 ? a : gcd(b, a % b);
}

static Frac
frac(int n)
{
    Frac ans;
    ans.num = n;
    ans.den = 1;
    return ans;
}

static Frac
reduce(Frac f)
{
    Frac ans;
    int g = gcd(f.num, f.den);
    ans.num = f.num / g;
    ans.den = f.den / g;
    if (ans.den < 0) {
        ans.num = -ans.num;
        ans.den = -ans.den;
    }
    return ans;
}

static Frac
add(Frac f1, Frac f2)
{
    Frac ans;
    ans.num = f1.num * f2.den + f1.den * f2.num;
    ans.den = f1.den * f2.den;
    return reduce(ans);
}

static Frac
neg(Frac f)
{
    Frac ans;
    ans.num = -f.num;
    ans.den = f.den;
    return reduce(ans);
}

static Frac
subtract(Frac f1, Frac f2)
{
    return add(f1, neg(f2));
}

static Frac
mult(Frac f1, Frac f2)
{
    Frac ans;
    ans.num = f1.num * f2.num;
    ans.den = f1.den * f2.den;
    return reduce(ans);
}

static Frac
inv(Frac f)
{
    Frac ans;
    ans.num = f.den;
    ans.den = f.num;
    return reduce(ans);
}

static Frac
div(Frac f1, Frac f2)
{
    return mult(f1, inv(f2));
}

/* Fraction IO library functions */

static Frac
recv_frac(envid_t from)
{
    Frac ans;
    ans.num = ipc_recv(0, 0, 0, from);
    ans.den = ipc_recv(0, 0, 0, from);
    return ans;
}

// Send the frac value to 'num' distinct other machines
static bool SENT[NENV];
static void
send_frac(Frac frac, int num)
{
    int i;
    for (i = 0; i < NENV; i++)
        SENT[i] = false;
    while (true) {
        for (i = 0; i < NENV; i++)
            if (envs[i].env_ipc_recving && !SENT[i] &&
                    envs[i].env_ipc_srcenv == thisenv->env_id) {
                ipc_send(envs[i].env_id, frac.num, 0, 0);
                ipc_send(envs[i].env_id, frac.den, 0, 0);
                SENT[i] = true;
                if (--num == 0)
                    return;
            }
        sys_yield();
    }
}

/* Convenience function for forking a child */

static envid_t
test_fork(void) {
    envid_t child;
    if ((child = fork()) < 0)
        panic("fork: %e", child);
    return child;
}

/* Operations on series */

// Output the series 'num' times
void split_series(envid_t in, int num)
{
    while (1) {
        Frac c = recv_frac(in);
        send_frac(c, num);
    }
}

// Multiply a series by a scalar 'scale'
void
scale_series(envid_t in, Frac scale)
{
    while (1) {
        Frac c = recv_frac(in);
        send_frac(mult(c, scale), 1);
    }
}

// Multiply a series by x^'shift'
void
shift_series(envid_t in, int shift)
{
    int i;
    for (i = 0; i < shift; i++)
        send_frac(frac(0), 1);
    while (1) {
        Frac c = recv_frac(in);
        send_frac(c, 1);
    }
}

// Add two series
void
add_series(envid_t in1, envid_t in2)
{
    while (1) {
        Frac c1 = recv_frac(in1);
        Frac c2 = recv_frac(in2);
        send_frac(add(c1, c2), 1);
    }
}

// Multiply two series
void
mult_series(envid_t in1, envid_t in2)
{
    Frac c1 = recv_frac(in1);
    Frac c2 = recv_frac(in2);
    send_frac(mult(c1, c2), 1);

    envid_t e_split1, e_split2, e_leftmult, e_rightmult, e_innermult, e_shift;

    if ((e_split1 = test_fork()) == 0)
        split_series(in1, 2);
    if ((e_split2 = test_fork()) == 0)
        split_series(in2, 2);
    if ((e_leftmult = test_fork()) == 0)
        scale_series(e_split1, c2);
    if ((e_rightmult = test_fork()) == 0)
        scale_series(e_split2, c1);
    if ((e_innermult = test_fork()) == 0)
        mult_series(e_split1, e_split2);
    if ((e_shift = test_fork()) == 0)
        shift_series(e_innermult, 1);

    while (1) {
        Frac c1 = recv_frac(e_leftmult);
        Frac c2 = recv_frac(e_rightmult);
        Frac c3 = recv_frac(e_shift);
        send_frac(add(add(c1, c2), c3), 1);
    }
}

// Compute the reciprocal of a series
void
inv_series(envid_t in)
{
    envid_t me = thisenv->env_id;

    Frac first = recv_frac(in);

    envid_t e_mult;

    if ((e_mult = test_fork()) == 0)
        mult_series(in, me);

    send_frac(inv(first), 2);

    while (1) {
        Frac c = recv_frac(e_mult);
        send_frac(neg(div(c, first)), 2);
    }
}

// Differentiate a series
void
differentiate_series(envid_t in)
{
    recv_frac(in);
    int i;
    for (i = 0; ; i++) {
        Frac c = recv_frac(in);
        send_frac(mult(c, frac(i + 1)), 1);
    }
}

// Integrate a series, with the provided initial constant term
void
integrate_series(envid_t in, Frac const_term)
{
    send_frac(const_term, 1);
    int i;
    for (i = 0; ; i++) {
        Frac c = recv_frac(in);
        send_frac(div(c, frac(i + 1)), 1);
    }
}

// Compute e^series
void exponentiate_series(envid_t in)
{
    envid_t me = thisenv->env_id;
    envid_t e_deriv, e_mult, e_integ;

    if ((e_deriv = test_fork()) == 0)
        differentiate_series(in);
    if ((e_mult = test_fork()) == 0)
        mult_series(me, e_deriv);
    if ((e_integ = test_fork()) == 0)
        integrate_series(e_mult, frac(1));

    while (1) {
        Frac c = recv_frac(e_integ);
        send_frac(c, 2);
    }
}

// Compute the sine of a series
// using the differential equation X = INT[ F' Y ] and Y = INT[ F' X ]
void sin_series(envid_t in)
{
    envid_t me = thisenv->env_id;
    envid_t e_deriv, e_split, e_neg_deriv, e_mult_sin, e_cos, e_mult_cos, e_sin;

    if ((e_deriv = test_fork()) == 0)
        differentiate_series(in);
    if ((e_split = test_fork()) == 0)
        split_series(e_deriv, 2);
    if ((e_neg_deriv = test_fork()) == 0)
        scale_series(e_split, frac(-1));
    if ((e_mult_sin = test_fork()) == 0)
        mult_series(e_neg_deriv, me);
    if ((e_cos = test_fork()) == 0)
        integrate_series(e_mult_sin, frac(1));
    if ((e_mult_cos = test_fork()) == 0)
        mult_series(e_split, e_cos);
    if ((e_sin = test_fork()) == 0)
        integrate_series(e_mult_cos, frac(0));

    while (1) {
        Frac c = recv_frac(e_sin);
        send_frac(c, 2);
    }
}

// Print the series to the console
void
print(envid_t in)
{
    int i = 0;
    bool printed = false;
    for (i = 0; ; i++) {
        Frac c = recv_frac(in);
        if (c.num == 0)
            continue;
        if (c.num > 0 && printed)
            cprintf("+");
        if (c.den == 1) {
            if (c.num == -1)
                cprintf("-");
            else if (c.num != 1)
                cprintf("%d ", c.num);
            else if (i == 0)
                cprintf("1 ");
        } else {
            cprintf("%d/%d ", c.num, c.den);
        }
        if (i == 1)
            cprintf("x ");
        else if (i > 1)
            cprintf("x^%d ", i);
        printed = true;
    }
}

void
umain(int argc, char **argv)
{
    envid_t me = thisenv->env_id;
    envid_t e_comp, e_print;

    if ((e_comp = test_fork()) == 0)
        sin_series(me);
    if ((e_print = test_fork()) == 0)
        print(e_comp);

    // Send x + x^3 = (0, 1, 0, 1, 0, 0, 0, ...) to the input stream
    send_frac(frac(0), 1);
    send_frac(frac(1), 1);
    send_frac(frac(0), 1);
    send_frac(frac(1), 1);
    while (1) {
        send_frac(frac(0), 1);
    }
}

