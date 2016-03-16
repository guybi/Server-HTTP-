// Guy Biton 305574709

#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TRUE 1
#define ERROR -1

// check if the number okay
int checkNumOfThreads(int num_threads_in_pool);

threadpool* create_threadpool(int num_threads_in_pool)
{
  /* check num of threads from the input*/
  int resultTest = checkNumOfThreads(num_threads_in_pool);
  if(resultTest == ERROR)
  {
    /*num of thread is not valid */
     return NULL;
  }

 threadpool* threadPool = (threadpool*)malloc(sizeof(threadpool));
 if(threadPool == NULL)
 {
   perror("malloc");
   return NULL;
 }
 threadPool-> num_threads = num_threads_in_pool; // init the number of threads
 threadPool-> qhead = NULL;   // queue head pointer to NULL
 threadPool-> qtail = NULL;   // queue tail pointer to NULL
 threadPool-> qsize = 0;      // queue size init to 0
 threadPool-> shutdown = 0;  // init shutdown to 0

 //(E)*******INIT LOCK AND CONDITION VARIAVBLES:************
 int retMutex; // get val from mutex init
 int retCondE; // get val from condition empty
 int retCondN; // get val from condition not empty

 // mutex init
 retMutex = pthread_mutex_init(&threadPool->qlock, NULL);
 if(retMutex != 0)
 {
   perror("ERROR  - init mutex  \n");
   free(threadPool);
   return NULL;
 }

 //cond init
 retCondN = pthread_cond_init(&threadPool->q_not_empty, NULL);
 if( retCondN != 0)
 {
   perror("ERROR - init cond \n");
   pthread_mutex_destroy(&threadPool->qlock);//mutex destroy
   free(threadPool);
   return NULL;
 }
 retCondE = pthread_cond_init(&threadPool->q_empty, NULL);
 if( retCondE != 0)
 {
   perror("ERROR - init cond \n");
   pthread_mutex_destroy(&threadPool->qlock);  //mutex destroy
   pthread_cond_destroy(&threadPool->q_not_empty); // cond destroy
   free(threadPool);
   return NULL;
 }
 //********************************************************
 //(F)SHUTDOWN = DONT_ACCEPT = 0
 threadPool->dont_accept = 0;
 //********************************************************
 //*Create the threads with do_work as execution function and the pool as an
 // argument*
 threadPool->threads = (pthread_t*)malloc(threadPool->num_threads*sizeof(pthread_t));
 if(threadPool->threads == NULL)
 {
   perror("ERROR: malloc() \n");
   pthread_mutex_destroy(&threadPool->qlock);
   pthread_cond_destroy(&threadPool->q_not_empty);
   pthread_cond_destroy(&threadPool->q_empty);
   free(threadPool);
   return NULL;
 }

  int i = 0;
  int run = 0;
  int resPT;
  for (;i < num_threads_in_pool;i++)
  {
    //sleep(2);
    resPT = pthread_create(threadPool->threads+i, NULL,do_work,threadPool);
    if(resPT != 0)
    {
        while(run<i)
        {
          if(pthread_cancel(threadPool->threads[run]) != 0)
          {
            fprintf(stderr, "ERROR - Cancel");
            return NULL;
          }
          run++;
        }
        if(pthread_mutex_destroy(&threadPool->qlock) != 0)
        {
            fprintf(stderr, "ERROR - destroy");
            return NULL;
        }
        if(pthread_cond_destroy(&threadPool->q_not_empty) != 0)
        {
          fprintf(stderr, "ERROR - destroy");
          return NULL;
        }
        if(pthread_cond_destroy(&threadPool->q_empty) != 0)
        {
          fprintf(stderr, "ERROR - destroy");
          return NULL;
        }
        if(pthread_cond_destroy(&threadPool->q_empty) != 0)
        {
          fprintf(stderr, "ERROR - destroy");
          return NULL;
        }
        free(threadPool->threads);
        free(threadPool);
        return NULL;
    }
  }
return threadPool;
}
////////////////////////////////////////////////////////////////////////////////
void* do_work(void* p)
{
  threadpool* tp = (threadpool*)p;


  if(tp == NULL || tp->threads == NULL)          // input type threadpool but empty
  {
    return NULL;
  }

  work_t*  work=NULL;
  while(TRUE)
  {
    if(pthread_mutex_lock(&tp->qlock) != 0)
    {
      fprintf(stderr, "ERROR \n" );
      return (void*) -1;
    }
    if(tp->shutdown == 1)
    {
      pthread_mutex_unlock(&tp->qlock);
      return NULL;
    }
    if(tp->qsize == 0)                  // the queue is empty
    {
      if( pthread_cond_wait(&tp->q_empty, &tp->qlock) != 0) // need to wait because the queue is empty
      {
        fprintf(stderr, "ERROR \n" );
        return (void*) -1;
      }
    }
    if(tp->shutdown == 1)
    {
      pthread_mutex_unlock(&tp->qlock);	// unlock
      return NULL;
    }
    // take the first element from the queue and start job
    if(tp->qsize == 0)
    {
        //pthread_cond_wait(&tp->q_empty, &tp->qlock); // need to wait because the queue is empty
      work = NULL;
    }
    if(tp->qsize == 1) // this is the last elemnent in queue.
    {
      work = tp->qhead;         //work = dequeue
      tp->qsize = tp->qsize -1; // update the queue size
      tp->qhead = NULL;
      tp->qtail = NULL;
    }
    else if(tp->qsize > 1)// this is not the last element in queue
    {
        tp->qsize = tp->qsize -1; // update the queue size
        work = tp->qhead;        //work = dequeue
        tp->qhead=tp->qhead->next;
    }
      if(tp->qsize == 0 && tp->dont_accept == 1)
      {
        if(pthread_cond_signal(&tp->q_not_empty) != 0)  // If the queue becomes empty and destruction
        {
          fprintf(stderr, "ERROR - Signal" );
          return (void*) -1;
        }                                              //process wait to begin, signal destruction
      }
      pthread_mutex_unlock(&tp->qlock);
        if(work != NULL)
         {
            (work->routine)(work->arg);         // routing function get new  work
             free(work);
        }
  }
  return NULL;
}
///////////////////////////////////////////////////////////////////////////////
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
  if(from_me == NULL || dispatch_to_here == NULL)
   {
    return;
  }
  if(pthread_mutex_lock(&from_me->qlock) != 0)
  {
    fprintf(stderr, "ERROR lock\n" );
    return;
  }
  if(from_me->threads == NULL)
  {
    if(pthread_mutex_unlock(&from_me->qlock) != 0)
    {
      fprintf(stderr, "ERROR - unlock\n" );
      return;
    }
    return;
  }

  // check if destruction has begun
  if(from_me->dont_accept==1)
  {
    if(pthread_mutex_unlock(&from_me->qlock) != 0)
    {
      fprintf(stderr, "ERROR - unlock\n" );
      return;
    }
    return;
  }
  // can enter work
  work_t* new_work = (work_t*)malloc(sizeof(work_t));
  if(new_work == NULL)
  {
    perror("malloc");
    if(pthread_mutex_unlock(&from_me->qlock) != 0)
    {
      fprintf(stderr, "ERROR - unlock\n" );
      return;
    }
    return;
  }
  new_work->arg = arg;
  new_work->next = NULL;
  new_work->routine = dispatch_to_here;   // enter routine to work

  // need to enqueue the new_work to list
  if(from_me-> qsize == 0) // the queue is empty
  {
    from_me->qhead = new_work;
    from_me->qtail = new_work;
  }
  else
  {
    from_me->qtail->next = new_work;
    from_me->qtail = new_work;
  }
  from_me->qsize++;
  if(pthread_cond_signal(&from_me->q_empty) != 0) // threads can take work
  {
    fprintf(stderr, "ERROR - Signal\n" );
    return;
  }
  if(pthread_mutex_unlock(&from_me->qlock) != 0)
  {
    fprintf(stderr, "ERROR - unlock\n" );
    return;
  }
}
/////////////////////////////////////////////////////////////////////////
/**
 * destroy_threadpool kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
 */
void destroy_threadpool(threadpool* destroyme)
{
  if(destroyme == NULL)
  {
    return;
  }
  if(pthread_mutex_lock(&destroyme->qlock) != 0)
  {
    fprintf(stderr, "ERROR - lock");
    free(destroyme->threads);
    free(destroyme);
    return;
  }
  destroyme->dont_accept = 1;        // update - dont accept

  if(destroyme->qsize != 0) // wait to all threads
  {
        // have some threads in work, in to wait until finish
        // the last thread wake up the thread that stop here and wait
        if(pthread_cond_wait(&destroyme->q_not_empty,&destroyme->qlock) != 0)
        {
          fprintf(stderr, "ERROR -  CondWait");
          free(destroyme->threads);
          free(destroyme);
          return;
        }
  }

  destroyme->shutdown = 1;
  if(pthread_cond_broadcast(&destroyme->q_empty) != 0) // need to wake up all the sleep threads
  {
    fprintf(stderr, "ERROR - broadCast");
    free(destroyme->threads);
    free(destroyme);
    return;
  }
  if(pthread_mutex_unlock(&destroyme->qlock) != 0)
  {
    fprintf(stderr, "ERROR - unlock");
    free(destroyme->threads);
    free(destroyme);
    return;
  }
  int run =0;
  int amount = destroyme->num_threads;
  while (run < amount) {
    if ( pthread_join(destroyme->threads[run],NULL) != 0)
    {
      fprintf(stderr, "ERROR - join");
      free(destroyme->threads);
      free(destroyme);
      return;
    }
    run ++;
  }
  if(pthread_mutex_destroy(&destroyme->qlock) != 0)
  {
    fprintf(stderr, "ERROR - destroy_mutex");
    free(destroyme->threads);
    free(destroyme);
    return;
  }
  if(pthread_cond_destroy(&destroyme->q_empty) != 0)
  {
    fprintf(stderr, "ERROR - destroy_cond");
    free(destroyme->threads);
    free(destroyme);
    return;
  }
  if( pthread_cond_destroy(&destroyme->q_not_empty) != 0)
  {
    fprintf(stderr, "ERROR - destroy_cond");
    free(destroyme->threads);
    free(destroyme);
    return;
  }
  free(destroyme->threads);
  free(destroyme);
  destroyme = NULL;
}
////////////////////////////////////////////////////////////////////////////////
int checkNumOfThreads(int num_threads_in_pool)
{
   if(num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL)
   {
     return ERROR;
   }
   return TRUE;
 }
 ///////////////////////////////////////////////////////////////////////////////
