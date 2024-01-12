#include<pthread.h>
#include<iostream>

using namespace std;

#define NUM_THREADS 5

void* say_hello(void* args)
{
  int num = *((int *)args);
  cout<<"Hello linux thread"<< num <<endl;
}

int main()
{
  pthread_t tids[NUM_THREADS]; //define id of thread

  for(int i = 0; i < NUM_THREADS; ++i)
  {
    int ret = pthread_create(&tids[i], NULL, say_hello, &i);


    if(ret != 0)
    {
      cout<<"create thead failed"<<endl;
    }
  }

  pthread_exit(NULL); //wait all threads execute complete 

  return 0;
}
