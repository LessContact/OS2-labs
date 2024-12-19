#ifndef SPEEN_H
#define SPEEN_H

#define STATUS_LOCK 0
#define STATUS_UNLOCK 1

typedef struct {
    volatile int lock;
} spinlock_t;

#endif //SPEEN_H
