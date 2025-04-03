//
// Created by student on 3/6/24.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> //for sleep
#include "threadpool.h"

work_t *dequeue(work_t **qhead, work_t **qtail);

void enqueue(work_t **qhead, work_t **qtail, work_t *new_work);

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

threadpool *create_threadpool(int num_threads_in_pool) {
    threadpool *tp = (threadpool *) malloc(sizeof(threadpool));
    if (tp == NULL) {
        perror("error: malloc\n");
        return NULL;
    }
    tp->threads = (pthread_t *) malloc(sizeof(pthread_t) * num_threads_in_pool);
    if (tp->threads == NULL) {
        free(tp);  // Free the previously allocated memory for threadpool
        perror("error: malloc\n");
        return NULL;
    }
    // Initialize the mutex
    if (pthread_mutex_init(&tp->qlock, NULL) != 0) {
        free(tp->threads);
        free(tp);  // Free the allocated memory for the threadpool
        perror("error: pthread_mutex_init\n");
        return NULL;
    }
    // Initialize the condition variable
    if (pthread_cond_init(&tp->q_empty, NULL) != 0 || pthread_cond_init(&tp->q_not_empty, NULL) != 0) {
        pthread_mutex_destroy(&tp->qlock);
        free(tp->threads);
        free(tp);  // Free the allocated memory for the threadpool
        perror("error: pthread_cond_init\n");
        return NULL;
    }

    tp->num_threads = num_threads_in_pool;
    tp->qsize = 0;
    tp->qhead = tp->qtail = NULL;
    tp->dont_accept = 0;
    tp->shutdown = 0;

    for (int i = 0; i < num_threads_in_pool; i++) { //create the threads - they live!
        if (pthread_create(&tp->threads[i], NULL, do_work, tp) != 0) {
            perror("pthread_create\n");

            free(tp->threads);
            pthread_mutex_destroy(&tp->qlock);
            pthread_cond_destroy(&tp->q_empty);
            pthread_cond_destroy(&tp->q_not_empty);
            free(tp);
            return NULL;
        }
    }
    return tp;
}

void *do_work(void *p) {
    threadpool* tp = (threadpool*)p;
    while (1) {
        pthread_mutex_lock(&tp->qlock);
        if (tp->shutdown) {
            pthread_mutex_unlock(&tp->qlock);
//             printf("Thread %lu exiting due to shutdown\n", pthread_self());
            pthread_exit(NULL);
        }
        // Must use a loop and not a condition because spurious wakeups are allowed with pthread_cond_wait.
        //That means a thread could wake up without the condition actually being true.
        while (tp->qsize == 0 && !tp->shutdown) {
            pthread_cond_wait(&tp->q_not_empty, &tp->qlock);
        }

        if (tp->shutdown) {//maybe the job was done while waiting - happens VERY SOON
            pthread_mutex_unlock(&tp->qlock);  //CRUCIAL to release the lock before exiting
//            printf("Thread %lu exiting due to shutdown\n", pthread_self());
            pthread_exit(NULL);
        }

        //if you come here, there is a job!!
        work_t *to_do = dequeue(&tp->qhead, &tp->qtail);
        tp->qsize--;
        if (tp->dont_accept && tp->qsize == 0) //last job was taken
            pthread_cond_signal(&tp->q_empty);
        pthread_mutex_unlock(&tp->qlock);

        //do the job!
//        printf("Thread %lu starting job\n", pthread_self());
        to_do->routine(to_do->arg);
//        printf("Thread %lu finished job\n", pthread_self());
        free(to_do); //don't need the work anymore
    }
}

void dispatch(threadpool *from_me, dispatch_fn dispatch_to_here, void *arg) {
    // Check from_me->dont accept and the enqueue the work inside the same lock section
    // That eliminates a race condition where the pool gets destroyed after the check but before the job is queued.

    work_t *new_work = (work_t *) malloc(sizeof(work_t));
    if (new_work == NULL) {
        error("error: malloc\n");
    }
    new_work->routine = dispatch_to_here;
    new_work->arg = arg;
    new_work->next = NULL;

    pthread_mutex_lock(&from_me->qlock);
    if (from_me->dont_accept){
        pthread_mutex_unlock(&from_me->qlock); // unlock
        free(new_work);
        return;
    }
    enqueue(&from_me->qhead, &from_me->qtail, new_work); //enter job to the queue
    from_me->qsize++;
    pthread_cond_signal(&from_me->q_not_empty);
    pthread_mutex_unlock(&from_me->qlock); // unlock
}

work_t *dequeue(work_t **qhead, work_t **qtail) {
    if (*qhead == NULL) { // Queue is empty
        return NULL;
    }
    work_t *dequeuedWork = *qhead;
    *qhead = (*qhead)->next;
    // If the queue becomes empty, update the tail as well
    if (*qhead == NULL) {
        *qtail = NULL;
    }
    return dequeuedWork;
}

void enqueue(work_t **qhead, work_t **qtail, work_t *new_work) {
    if (*qtail == NULL) {
        // If the queue is empty, set both head and tail to the new work item
        *qhead = *qtail = new_work;
    } else {
        // Otherwise, add the new work item to the end of the queue
        (*qtail)->next = new_work;
        *qtail = new_work;
    }
}

void destroy_threadpool(threadpool *destroyme) {
    pthread_mutex_lock(&destroyme->qlock);
    destroyme->dont_accept = 1;  // Set don't_accept flag to 1

    while (destroyme->qsize > 0) {
        pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);  // Wait for the queue to become empty
    }
    destroyme->shutdown = 1;  // Set shutdown flag to 1
    pthread_cond_broadcast(&destroyme->q_not_empty);  // Signal threads waiting on the queue
    pthread_mutex_unlock(&destroyme->qlock);

    for (int i = 0; i < destroyme->num_threads; i++) {
        pthread_join(destroyme->threads[i], NULL);  // Join all threads
    }
    pthread_mutex_destroy(&destroyme->qlock);  // Destroy the mutex
    pthread_cond_destroy(&destroyme->q_empty);  // Destroy the condition variable for an empty queue
    pthread_cond_destroy(&destroyme->q_not_empty);  // Destroy the condition variable for a non-empty queue

    free(destroyme->threads);  // Free the memory allocated for the threads
    free(destroyme);  // Free the memory allocated for the threadpool
}
