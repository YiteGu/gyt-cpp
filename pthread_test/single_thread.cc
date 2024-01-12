#include<pthread.h>
#include<iostream>

using namespace std;

void* say_hello(void* args)
{
  cout<<"Hello linux thread"<<endl;
}

int main()
{
  pthread_t tid; //define id of thread

  int ret = pthread_create(&tid, NULL, say_hello, NULL);

  if(ret != 0)
  {
    cout<<"create thead failed"<<endl;
  }

  pthread_exit(NULL);

  return 0;
}
