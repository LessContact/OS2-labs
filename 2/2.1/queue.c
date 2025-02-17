#define _GNU_SOURCE
#include <pthread.h>
#include <assert.h>

#include "queue.h"

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
}

int queue_add(queue_t *q, int val) {
	q->add_attempts++;

	assert(q->count <= q->max_count);

	if (q->count == q->max_count)
		return 0;

	qnode_t *new = malloc(sizeof(qnode_t));
	if (!new) {
		printf("Cannot allocate memory for new node\n");
		abort();
	}

	new->val = val;
	new->next = NULL;

	// error scheme:
	// writer записал 0, пытается ещё записать 1(попадает в else), но тут его время кончается
	// затем, reader прочитал 0(и убрал q->first, в то время как о q->last он не знает),
	// и тут writer проснулся, записав 1 в last = new(но first у нас теперь NULL)
	// writer пытается записать теперь 2, q->first is NULL, а в q->last у нас 1 лежит, мы благополучно затираем это
	// reader просыпается и считывает 2 вместо пропавшей 1.

	if (!q->first)
		q->first = q->last = new;
	else { // maybe also if
		//it goes into this branch and the getter reads the first and only element
		//it may segfault?
		q->last->next = new;
		q->last = q->last->next;
	}

	q->count++; // the count is pretty much always broken
	q->add_count++;

	return 1;
}

int queue_get(queue_t *q, int *val) {
	q->get_attempts++;

	assert(q->count >= 0);

	if (q->count == 0)
		return 0;
	// there is a chance that the q->count is actually 0
	// but the q->count will not reflect that so tmp will result in NULL
	qnode_t *tmp = q->first;
	*val = tmp->val;
	q->first = q->first->next;

	free(tmp);
	q->count--;
	q->get_count++;

	return 1;
}

void queue_print_stats(queue_t *q) {
	printf("queue stats: current size %d; attempts: (%ld %ld %ld); counts (%ld %ld %ld)\n",
		q->count,
		q->add_attempts, q->get_attempts, q->add_attempts - q->get_attempts,
		q->add_count, q->get_count, q->add_count -q->get_count);
}

