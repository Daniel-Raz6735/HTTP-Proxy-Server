/*Daniel Raz*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "threadpool.h"


/**
 * create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
 * this function should:
 * 1. input sanity check 
 * 2. initialize the threadpool structure
 * 3. initialized mutex and conditional variables
 * 4. create the threads, the thread init function is do_work and its argument is the initialized threadpool. 
 */
threadpool *create_threadpool(int num_threads_in_pool)
{
    if (num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL)
    {
        perror("iiligal pool size");
        return NULL;
    }
    threadpool *thpool = (threadpool *)malloc(sizeof(threadpool));
    if (thpool == NULL)
    {
        perror("error: malloc\n");
        return NULL;
    }

    thpool->num_threads = num_threads_in_pool;
    thpool->dont_accept = 0;
    thpool->shutdown = 0;
    thpool->qsize = 0;
    thpool->qhead = NULL;
    thpool->qtail = NULL;
    pthread_mutex_init(&thpool->qlock, NULL);
    pthread_cond_init(&thpool->q_not_empty, NULL);
    pthread_cond_init(&thpool->q_empty, NULL);

    thpool->threads = (pthread_t *)malloc(num_threads_in_pool * sizeof(pthread_t));
    if (thpool->threads == NULL)
    {
        perror("error:malloc\n");
        return NULL;
    }

    for (int i = 0; i < num_threads_in_pool; i++)
    {
        int x = pthread_create(&thpool->threads[i], NULL, do_work, thpool);
        if (x < 0)
        {
            perror("error:pthread_create\n");
            break;
        }
    }

    return thpool;
}

/**
 * dispatch enter a "job" of type work_t into the queue.
 * when an available thread takes a job from the queue, it will
 * call the function "dispatch_to_here" with argument "arg".
 * this function should:
 * 1. create and init work_t element
 * 2. lock the mutex
 * 3. add the work_t element to the queue
 * 4. unlock mutex
 *
 */
void dispatch(threadpool *from_me, dispatch_fn dispatch_to_here, void *arg)
{
    if (from_me == NULL || dispatch_to_here == NULL)
    {
        perror("here:\n");
        return;
    }

    pthread_mutex_lock(&from_me->qlock); //critical section
    if (from_me->dont_accept == 1)
    {
        pthread_mutex_unlock(&from_me->qlock);
        return;
    }

    work_t *job = (work_t *)malloc(sizeof(work_t));
    if (job == NULL)
    {
        perror("error:malloc\n");
        return;
    }

    job->arg = arg;
    job->routine = dispatch_to_here;
    job->next = NULL;

    if (from_me->qhead == NULL)
    {
        from_me->qhead = job;
        from_me->qtail = job;
        from_me->qsize++;
    }
    else
    {
        from_me->qtail->next = job;
        from_me->qtail = from_me->qtail->next;
        from_me->qsize++;
    }
    pthread_cond_signal(&from_me->q_not_empty);
    pthread_mutex_unlock(&from_me->qlock); //end of critical section
}

/**
 * The work function of the thread
 * this function should:
 * 1. lock mutex
 * 2. if the queue is empty, wait
 * 3. take the first element from the queue (work_t)
 * 4. unlock mutex
 * 5. call the thread routine
 *
 */
void *do_work(void *p)
{
    if (p == NULL)
        return NULL;

    threadpool *thpool = (threadpool *)p;

    while (1)
    {
        pthread_mutex_lock(&thpool->qlock);
        if (thpool->shutdown == 1)
        {
            pthread_mutex_unlock(&thpool->qlock);
            return NULL;
        }

        if (thpool->qsize == 0)
            pthread_cond_wait(&thpool->q_not_empty, &thpool->qlock);

        if (thpool->shutdown == 1)
        {
            pthread_mutex_unlock(&thpool->qlock);
            return NULL;
        }

        work_t *temp = thpool->qhead;
        if(temp == NULL)
        {
            pthread_mutex_unlock(&thpool->qlock);
            continue;    
        }
        thpool->qsize--;
        thpool->qhead = thpool->qhead->next;
        pthread_mutex_unlock(&thpool->qlock);
        temp->routine(temp->arg);
        free(temp);

        pthread_mutex_lock(&thpool->qlock);
        if (thpool->qsize == 0 && thpool->dont_accept == 1)
            pthread_cond_signal(&thpool->q_empty);
        pthread_mutex_unlock(&thpool->qlock);
    }
}

/**
 * destroy_threadpool kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
 */
void destroy_threadpool(threadpool *destroyme)
{
    if (destroyme == NULL)
        return;

    pthread_mutex_lock(&destroyme->qlock);
    destroyme->dont_accept = 1;
    if (destroyme->qsize > 0)
        pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);
    destroyme->shutdown = 1;
    pthread_cond_broadcast(&destroyme->q_not_empty);
    pthread_mutex_unlock(&destroyme->qlock);

    for (int i = 0; i < destroyme->num_threads; i++)
        pthread_join(destroyme->threads[i], NULL);

    free(destroyme->threads);
    pthread_mutex_destroy(&destroyme->qlock);
    pthread_cond_destroy(&destroyme->q_empty);
    pthread_cond_destroy(&destroyme->q_not_empty);
    free(destroyme);    
}

