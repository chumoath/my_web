#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>

void *worker1(void * arg){
    char c;
    read(0, &c, 1);

    while(1)
        printf("tid = %d: hello world\n", pthread_self());
}

void *worker2(void * arg){
    while(1)
        printf("tid = %d: hello world\n", pthread_self());
}

int main(int argc, char const *argv[])
{
    pthread_t tid;
    pthread_create(&tid, NULL, worker1, NULL);
    pthread_create(&tid, NULL, worker2, NULL);
    
    sleep(10000);
    return 0;
}
