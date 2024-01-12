#include<pthread.h>
#include<iostream>

using namespace std;

#define NUM_THREADS 5

int shard_queue = 0;

pthread_mutex_t shard_queue_lock;

void* say_hello(void* args)
{
  int num = *((int *)args);
  cout<<"Hello linux thread"<< num <<endl;

  pthread_mutex_lock(&shard_queue_lock); // lock

  cout<<"I am thread "<< num <<", the shard_queue is "<< shard_queue <<endl;

  shard_queue++;

  cout<<"I am thread "<< num <<", the shard_queue is "<< shard_queue <<" after ++"<<endl;

  pthread_mutex_unlock(&shard_queue_lock); // unlock

  pthread_exit(0);
}

int main()
{
  pthread_t tids[NUM_THREADS]; //define id of thread

  pthread_attr_t attr;

  pthread_attr_init(&attr);

  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE); // set thread joinable feature

  pthread_mutex_init(&shard_queue_lock, NULL);

  for(int i = 0; i < NUM_THREADS; ++i)
  {
    int ret = pthread_create(&tids[i], &attr, say_hello, &i);
  
    if(ret != 0)
    {
      cout<<"create thead failed"<< ret <<endl;
    }
  }

  pthread_attr_destroy(&attr); //release attr

  void* status;

  for(int j = 0; j < NUM_THREADS; ++j)
  {
    int ret = pthread_join(tids[j], &status); // block main thread, wait for sub thread complete
    if(ret != 0)
    {
      cout<<"pthread join error"<< ret <<endl;
    }
  }

  pthread_mutex_destroy(&shard_queue_lock);

  return 0;
}
