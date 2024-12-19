#ifndef MUTEX_H
#define MUTEX_H

#define STATUS_LOCK 0
#define STATUS_UNLOCK 1

typedef struct {
    int lock;
} mutex_t;

#endif //MUTEX_H
