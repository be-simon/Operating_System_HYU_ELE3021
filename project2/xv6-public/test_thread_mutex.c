#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#define THREAD_NUMBER 10

const int THREADMAX = 100000;
int global_counter;
pthread_mutex_t mutex;

void* couting(void* arg) {
  int local_times = *(int*)arg;
  int64_t local_counter = 0;
  int i;

  for (i = 0 ; i < local_times ; i++) {
    pthread_mutex_lock(&mutex);
    global_counter++;
    pthread_mutex_unlock(&mutex);
    local_counter++;
  }

  pthread_exit((void*)local_counter);
  return NULL; //not executed
}

int main(int argc, char * argv[]) {
  int i;
  pthread_t pid[THREAD_NUMBER];
  int ret_vals[THREAD_NUMBER];
  int thread_args[THREAD_NUMBER];

  pthread_mutex_init(&mutex, NULL);

  for (i = 0 ; i < THREAD_NUMBER ; i++) {
    thread_args[i] = THREADMAX * (i + 1);
    pthread_create(&pid[i], NULL, couting, (void*)&thread_args[i]);
  }

  for (i = 0 ; i < THREAD_NUMBER ; i++) {
    pthread_join(pid[i], (void*)&ret_vals[i]);
  }

  for (i = 0 ; i < THREAD_NUMBER ; i++) {
    printf("Local Counter : %d\n", ret_vals[i]);
  }

  printf("Global Counter : %d\n", global_counter);

  return 0;
}
