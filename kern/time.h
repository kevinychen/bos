#ifndef JOS_KERN_TIME_H
#define JOS_KERN_TIME_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/string.h>

void time_init(void);
void time_tick(void);
void cmostime(struct rtcdate*);
unsigned int time_msec(void);

#endif /* JOS_KERN_TIME_H */
