#include<pthread.h>
#include<iostream>

using namespace std;

#define NUM_SWITCH 5

int tasks = 10; // tasks num

pthread_mutex_t tasks_mutex;
pthread_cond_t tasks_cond;

void* work_thread2(void* args)
{
  int num = *((int *)args);

  pthread_t pid = pthread_self(); //get current thread id

  bool is_signaled = false;

  while(true)
  {
    pthread_mutex_lock(&tasks_mutex);

    if(tasks > NUM_SWITCH)
    {
      cout<<"current pid is ["<< pid <<"], tasks current is ["<< tasks <<"]"<<endl;
      --tasks;
    }

    if(!is_signaled)
    {
      cout<<"pthread cond signaled in thread "<< num <<endl;
      pthread_cond_signal(&tasks_cond);
      is_signaled = true;
    }

    pthread_mutex_unlock(&tasks_mutex);
    if(tasks == 0)
      break;
  }
}

void* work_thread1(void* args)
{
  int num = *((int *)args);

  pthread_t pid = pthread_self(); //get current thread id

  while(true)
  {
    pthread_mutex_lock(&tasks_mutex);

    if(tasks > NUM_SWITCH)
    {
      cout<<"current pid is ["<< pid <<"], pthread cond wait in thread "<< num <<endl;
      
      pthread_cond_wait(&tasks_cond, &tasks_mutex); //blocked current thread, until get the signal

      cout<<"wake up in thread "<< num <<endl;
    }
    else
    {
      cout<<"current pid is ["<< pid <<"], tasks current is ["<< tasks <<"]"<<endl;
      --tasks;
    }

    pthread_mutex_unlock(&tasks_mutex);
    if(tasks == 0)
      break;
  }
}

int main()
{
  pthread_t tid1, tid2; //define id of thread

  pthread_attr_t attr;

  pthread_attr_init(&attr);

  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE); // set thread joinable feature

  pthread_mutex_init(&tasks_mutex, NULL);

  pthread_cond_init(&tasks_cond, NULL);

  int index1 = 1;

  int ret = pthread_create(&tid1, &attr, work_thread1, &index1);
  
  if(ret != 0)
  {
      cout<<"create thead failed"<< ret <<endl;
  }

  int index2 = 2;

  ret = pthread_create(&tid2, &attr, work_thread2, &index2);
  if(ret != 0)
  {
      cout<<"create thead failed"<< ret <<endl;
  }

  void* status;

  pthread_join(tid1, &status); // block main thread, wait for sub thread complete

  pthread_join(tid2, &status); // block main thread, wait for sub thread complete

  pthread_attr_destroy(&attr); //release attr

  pthread_mutex_destroy(&tasks_mutex); //release lock

  pthread_cond_destroy(&tasks_cond); //release cond

  return 0;
}
