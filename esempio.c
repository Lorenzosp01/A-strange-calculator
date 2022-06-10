#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
pthread_cond_t cond;
pthread_mutex_t mutex;

void* funcThread(){
    sleep(5);
    printf("Ciao\n");
    fflush(stdout);
    pthread_cond_signal(&cond);
}

int main(int argc, char const *argv[])
{
    pthread_t tids[2];

    if(pthread_mutex_init(&mutex, NULL) == 0)
    {
        if(pthread_cond_init(&cond, NULL) == 0)
        {
            pthread_create(&tids[0], 0, funcThread, NULL);
            pthread_cond_wait(&cond, &mutex);
            printf("Sto per creare il secondo thread");
            fflush(stdout);
            pthread_create(&tids[1], 0, funcThread, NULL);
            printf("Ciao sono il secondo thread");
            fflush(stdout);
        }
    }

}
