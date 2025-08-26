#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <stdio.h>
#include <errno.h>
#include <time.h>

#define PROG_NAME "thread"
#define DEBUG
#ifdef DEBUG
#define dbg(fmt, ...) \
	printf("-- %s: %s: " fmt " \n", PROG_NAME, __func__, ##__VA_ARGS__)
#else
#define dbg(fmt, ...)
#endif

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;

	struct thread_data* th_data = (struct thread_data *) thread_param;
	th_data->thread_complete_success = false;

	// sleep and lock
	if (usleep(th_data->wait_obtain * 1000)) {
		perror("threadfunc: usleep (1)");
		goto out_unlocked;
	}
	pthread_mutex_lock(th_data->mutex);

	// sleep and unlock (eventually)
	if (usleep(th_data->wait_release * 1000)) {
		perror("threadfunc: usleep (1)");
		goto out_locked;
	}

	th_data->thread_complete_success = true;

out_locked:
	pthread_mutex_unlock(th_data->mutex);
out_unlocked:
	dbg("Exiting..");
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
	dbg("input data: obt:%d / wait: %d", wait_to_obtain_ms, wait_to_release_ms);
	struct thread_data *th_data = malloc(sizeof(struct thread_data));
	int ret;

	th_data->mutex = mutex;
	th_data->wait_obtain = wait_to_obtain_ms;
	th_data->wait_release = wait_to_release_ms;

	ret = pthread_create(thread, NULL, threadfunc, (void*)th_data);

	if (ret) {
		perror("start_thread_obtaining_mutex: pthread_create");
		exit (1);
	}

	fprintf(stderr, "Thread exited with %d", ret);

	return true;
}

