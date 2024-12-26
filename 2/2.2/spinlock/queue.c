#define _GNU_SOURCE
#include <pthread.h>
#include <assert.h>

#include "queue.h"

pthread_spinlock_t spinlock;

int init_spin_lock() {
	if(pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE))
		return -1;
}

void *qmonitor(void *arg) {
	queue_t *q = (queue_t *)arg;

	printf("qmonitor: [%d %d %d]\n", getpid(), getppid(), gettid());

	while (1) {
		queue_print_stats(q);
		sleep(1);
	}

	return NULL;
}

queue_t* queue_init(int max_count) {
	int err;

	queue_t *q = malloc(sizeof(queue_t));
	if (!q) {
		printf("Cannot allocate memory for a queue\n");
		abort();
	}

	q->first = NULL;
	q->last = NULL;
	q->max_count = max_count;
	q->count = 0;

	q->add_attempts = q->get_attempts = 0;
	q->add_count = q->get_count = 0;

	err = pthread_create(&q->qmonitor_tid, NULL, qmonitor, q);
	if (err) {
		printf("queue_init: pthread_create() failed: %s\n", strerror(err));
		abort();
	}

	return q;
}

void queue_destroy(queue_t *q) {
	pthread_cancel(q->qmonitor_tid);
	void* ret_val;
	int err = pthread_join(q->qmonitor_tid, &ret_val);
	if (err)
		fprintf(stderr, "queue_destroy: pthread_join() failed %s\n", strerror(err));

	qnode_t *cur_ptr_q = q->first;
	qnode_t *next_ptr_q;
	while (cur_ptr_q != NULL) {
		next_ptr_q = cur_ptr_q->next;
		free(cur_ptr_q);
		cur_ptr_q = next_ptr_q;
	}
	free(q);
	pthread_spin_destroy(&spinlock);
}

int queue_add(queue_t *q, int val) {
	qnode_t *new = malloc(sizeof(qnode_t));
	if (!new) {
		printf("malloc: cannot allocate memory for new node\n");
		abort();
	}
	new->val = val;
	new->next = NULL;

	q->add_attempts++;
	pthread_spin_lock(&spinlock);

	assert(q->count <= q->max_count);

	if (q->count == q->max_count) {
		free(new);
		pthread_spin_unlock(&spinlock);
		return 0;
	}

	if (!q->first)
		q->first = q->last = new;
	else {
		q->last->next = new;
		q->last = q->last->next;
	}

	q->count++;
	q->add_count++;
	pthread_spin_unlock(&spinlock);
	return 1;
}

int queue_get(queue_t *q, int *val) {
	q->get_attempts++;
	pthread_spin_lock(&spinlock);
	assert(q->count >= 0);
	if (q->count == 0) {
		pthread_spin_unlock(&spinlock);
		return 0;
	}
	qnode_t *tmp = q->first;
	q->first = q->first->next;
	q->count--;
	q->get_count++;
	pthread_spin_unlock(&spinlock);
	*val = tmp->val;
	free(tmp);
	return 1;
}

void queue_print_stats(queue_t *q) {
	printf("queue stats: current size %d; attempts: (%ld %ld %ld); counts (%ld %ld %ld)\n",
		q->count,
		q->add_attempts, q->get_attempts, q->add_attempts - q->get_attempts,
		q->add_count, q->get_count, q->add_count -q->get_count);
}

