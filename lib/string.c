// Basic string routines.  Not hardware optimized, but not shabby.

#include <inc/string.h>
#include <inc/stdio.h>


// Using assembly for memset/memmove
// makes some difference on real hardware,
// but it makes an even bigger difference on bochs.
// Primespipe runs 3x faster this way.
#define ASM 1

int
strlen(const char *s)
{
	int n;

	for (n = 0; *s != '\0'; s++)
		n++;
	return n;
}

int
strnlen(const char *s, size_t size)
{
	int n;

	for (n = 0; size > 0 && *s != '\0'; s++, size--)
		n++;
	return n;
}

char *
strcpy(char *dst, const char *src)
{
	char *ret;

	ret = dst;
	while ((*dst++ = *src++) != '\0')
		/* do nothing */;
	return ret;
}

char *
strcat(char *dst, const char *src)
{
	int len = strlen(dst);
	strcpy(dst + len, src);
	return dst;
}

char *
strncpy(char *dst, const char *src, size_t size) {
	size_t i;
	char *ret;

	ret = dst;
	for (i = 0; i < size; i++) {
		*dst++ = *src;
		// If strlen(src) < size, null-pad 'dst' out to 'size' chars
		if (*src != '\0')
			src++;
	}
	return ret;
}

size_t
strlcpy(char *dst, const char *src, size_t size)
{
	char *dst_in;

	dst_in = dst;
	if (size > 0) {
		while (--size > 0 && *src != '\0')
			*dst++ = *src++;
		*dst = '\0';
	}
	return dst - dst_in;
}

int
strcmp(const char *p, const char *q)
{
	while (*p && *p == *q)
		p++, q++;
	return (int) ((unsigned char) *p - (unsigned char) *q);
}

int
strncmp(const char *p, const char *q, size_t n)
{
	while (n > 0 && *p && *p == *q)
		n--, p++, q++;
	if (n == 0)
		return 0;
	else
		return (int) ((unsigned char) *p - (unsigned char) *q);
}

// Return a pointer to the first occurrence of 'c' in 's',
// or a null pointer if the string has no 'c'.
char *
strchr(const char *s, char c)
{
	for (; *s; s++)
		if (*s == c)
			return (char *) s;
	return 0;
}

// Return a pointer to the first occurrence of 'c' in 's',
// or a pointer to the string-ending null character if the string has no 'c'.
char *
strfind(const char *s, char c)
{
	for (; *s; s++)
		if (*s == c)
			break;
	return (char *) s;
}

#if ASM
void *
memset(void *v, int c, size_t n)
{
	char *p;

	if (n == 0)
		return v;
	if ((int)v%4 == 0 && n%4 == 0) {
		c &= 0xFF;
		c = (c<<24)|(c<<16)|(c<<8)|c;
		asm volatile("cld; rep stosl\n"
			:: "D" (v), "a" (c), "c" (n/4)
			: "cc", "memory");
	} else
		asm volatile("cld; rep stosb\n"
			:: "D" (v), "a" (c), "c" (n)
			: "cc", "memory");
	return v;
}

void *
memmove(void *dst, const void *src, size_t n)
{
	const char *s;
	char *d;

	s = src;
	d = dst;
	if (s < d && s + n > d) {
		s += n;
		d += n;
		if ((int)s%4 == 0 && (int)d%4 == 0 && n%4 == 0)
			asm volatile("std; rep movsl\n"
				:: "D" (d-4), "S" (s-4), "c" (n/4) : "cc", "memory");
		else
			asm volatile("std; rep movsb\n"
				:: "D" (d-1), "S" (s-1), "c" (n) : "cc", "memory");
		// Some versions of GCC rely on DF being clear
		asm volatile("cld" ::: "cc");
	} else {
		if ((int)s%4 == 0 && (int)d%4 == 0 && n%4 == 0)
			asm volatile("cld; rep movsl\n"
				:: "D" (d), "S" (s), "c" (n/4) : "cc", "memory");
		else
			asm volatile("cld; rep movsb\n"
				:: "D" (d), "S" (s), "c" (n) : "cc", "memory");
	}
	return dst;
}

#else

void *
memset(void *v, int c, size_t n)
{
	char *p;
	int m;

	p = v;
	m = n;
	while (--m >= 0)
		*p++ = c;

	return v;
}

void *
memmove(void *dst, const void *src, size_t n)
{
	const char *s;
	char *d;

	s = src;
	d = dst;
	if (s < d && s + n > d) {
		s += n;
		d += n;
		while (n-- > 0)
			*--d = *--s;
	} else
		while (n-- > 0)
			*d++ = *s++;

	return dst;
}
#endif

void *
memcpy(void *dst, const void *src, size_t n)
{
	return memmove(dst, src, n);
}

int
memcmp(const void *v1, const void *v2, size_t n)
{
	const uint8_t *s1 = (const uint8_t *) v1;
	const uint8_t *s2 = (const uint8_t *) v2;

	while (n-- > 0) {
		if (*s1 != *s2)
			return (int) *s1 - (int) *s2;
		s1++, s2++;
	}

	return 0;
}

void *
memfind(const void *s, int c, size_t n)
{
	const void *ends = (const char *) s + n;
	for (; s < ends; s++)
		if (*(const unsigned char *) s == (unsigned char) c)
			break;
	return (void *) s;
}

long
strtol(const char *s, char **endptr, int base)
{
	int neg = 0;
	long val = 0;

	// gobble initial whitespace
	while (*s == ' ' || *s == '\t')
		s++;

	// plus/minus sign
	if (*s == '+')
		s++;
	else if (*s == '-')
		s++, neg = 1;

	// hex or octal base prefix
	if ((base == 0 || base == 16) && (s[0] == '0' && s[1] == 'x'))
		s += 2, base = 16;
	else if (base == 0 && s[0] == '0')
		s++, base = 8;
	else if (base == 0)
		base = 10;

	// digits
	while (1) {
		int dig;

		if (*s >= '0' && *s <= '9')
			dig = *s - '0';
		else if (*s >= 'a' && *s <= 'z')
			dig = *s - 'a' + 10;
		else if (*s >= 'A' && *s <= 'Z')
			dig = *s - 'A' + 10;
		else
			break;
		if (dig >= base)
			break;
		s++, val = (val * base) + dig;
		// we don't properly detect overflow!
	}

	if (endptr)
		*endptr = (char *) s;
	return (neg ? -val : val);
}

// Convert rtcdate to time, in sec after 2000
time_t
rtcdate_to_time(struct rtcdate *r)
{
    time_t val = 0, year;

    for (year = 0; year < r->year % 100; year++)
        val += 365 + (year % 4 == 0);

    switch (r->month - 1) {
        case 11: val += 30;
        case 10: val += 31;
        case 9: val += 30;
        case 8: val += 31;
        case 7: val += 31;
        case 6: val += 30;
        case 5: val += 31;
        case 4: val += 30;
        case 3: val += 31;
        case 2: val += 28 + (r->year % 4 == 0);
        case 1: val += 31;
    }

    val += r->day - 1;
    val = 24 * val + r->hour;
    val = 60 * val + r->minute;
    val = 60 * val + r->second;

    return val;
}

// Convert time to rtcdate
void
time_to_rtcdate(time_t time, struct rtcdate *r)
{
    r->second = time % 60;
    time /= 60;
    r->minute = time % 60;
    time /= 60;
    r->hour = time % 24;
    time /= 24;

    // mod by number of days in 400 years
    time %= 400 * 365 + 100 - 4 + 1;

    r->year = 2000;
    while (365 + (r->year % 4 == 0) <= time) {
        time -= 365 + (r->year % 4 == 0);
        r->year++;
    }

    time_t DAYS_IN_MONTH[] =
        {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    DAYS_IN_MONTH[1] += r->year % 4 == 0;

    r->month = 1;
    while (time > DAYS_IN_MONTH[r->month])
        time -= DAYS_IN_MONTH[r->month++];

    r->day = time + 1;
}

static time_t
subtract_time(time_t time, char type, time_t val, struct rtcdate *r)
{
    time_t mult = 1;
    int newVal;

    switch (type) {
        case 'y':
        case 'Y':
            r->year += 100 * (val > r->year) - val % 100;
            mult = 0;
            break;
        case 'n':  // month
        case 'N':
            newVal = val / 12 + (val >= r->month);
            if ((time = subtract_time(time, 'y', newVal, r)) == 0)
                return time;
            r->month += 12 * (val >= r->month) - val % 12;
            mult = 0;
            break;
        case 'd':
        case 'D':
            mult *= 24;
        case 'h':
        case 'H':
            mult *= 60;
        case 'm':
        case 'M':
            mult *= 60;
        case 's':
        case 'S':
            break;
        default:
            return 0;
    }

    time -= mult * val;
    time_to_rtcdate(time, r);
    return time;
}

static time_t
parse_relative_time(const char *str, time_t current)
{
    // Parse [time][unit][time][unit]...
    //   e.g. 5s, 2m, 2m5s
    struct rtcdate r;
    time_to_rtcdate(current, &r);

    char *endptr;
    do {
        long val = strtol(str, &endptr, 10);
        if (!strchr("smhdny", *endptr))
            return 0;
        if ((current = subtract_time(current, *endptr, val, &r)) == 0)
            return 0;
        str = endptr + 1;
    }
    while (*str);

    return current;
}

// Convert string to rtcdate.
// Forms:
//   c[time]
//   YYYY (ISO)
//   YYYY-MM (ISO)
//   YYYY-MM-DD (ISO)
//   YYYY-MM-DDThh:mm (ISO)
//   YYYY-MM-DDThh:mm:ss (ISO)
//   YYYY-MM-DDThh:mm[A/P]M
//   YYYY-MM-DDThh:mm:ss[A/P]M
//   [time][unit] (e.g. 5s, 2m)
// Returns 0 on error, or time on success
time_t
parse_time(const char *str, time_t current)
{
    char *endptr;
    time_t time;

    if (*str == '\0')
        return current;
    if (*str == 'c') {
        // form c[time]
        time = strtol(str + 1, &endptr, 10);
        return *endptr == 0 ? time : 0;
    }
    if ((time = parse_relative_time(str, current))) {
        // form [time][unit]
        return time;
    }

    struct rtcdate r;
    r.year = r.month = r.day = r.hour = r.minute = r.second = 0;

    r.year = strtol(str, &endptr, 10);
    if (*endptr == '\0') {
        // form YYYY
        return rtcdate_to_time(&r);
    }
    if (*endptr != '-')
        return 0;

    str = endptr + 1;
    r.month = strtol(str, &endptr, 10);
    if (*endptr == '\0') {
        // form YYYY-MM
        return rtcdate_to_time(&r);
    }
    if (*endptr != '-')
        return 0;

    str = endptr + 1;
    r.day = strtol(str, &endptr, 10);
    if (*endptr == '\0') {
        // form YYYY-MM-DD
        return rtcdate_to_time(&r);
    }
    
    str = endptr + 1;
    r.hour = strtol(str, &endptr, 10);
    if (*endptr != ':')
        return 0;

    str = endptr + 1;
    r.minute = strtol(str, &endptr, 10);
    if (*endptr == '\0') {
        // form YYYY-MM-DDThh:mm
        return rtcdate_to_time(&r);
    }

    if (*endptr == ':') {
        str = endptr + 1;
        r.second = strtol(str, &endptr, 10);
        if (*endptr == '\0') {
            // form YYYY-MM-DDThh:mm:ss
            return rtcdate_to_time(&r);
        }
    }

    if (strchr("AP", *endptr) && *(endptr + 1) == 'M'
            && *(endptr + 2) == '\0') {
        // form YYYY-MM-DDThh:mm[A/P]M
        if ((*endptr == 'A') ^ (r.hour % 12))
            r.hour = (r.hour + 12) % 24;
        return rtcdate_to_time(&r);
    }

    return 0;
}

