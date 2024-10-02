 
/* Parte 3 - PPC
    Implementação do modelo produtor consumidor utilizando threads e MPI
    compilar: mpicc -o part3 part3.c -lpthread
    executar: mpiexec -n 3 ./part3
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  
#include <mpi.h>
#include <pthread.h> 
#include <unistd.h>
#include <semaphore.h>
#include <time.h>

#define BUFFER_SIZE 4 // Númermo máximo de relogios enfileirados

typedef struct Clock { 
   int p[3];
} Clock;

//filas
Clock rcv[BUFFER_SIZE];
Clock snd[BUFFER_SIZE];

//contadores
int rcv_count = 0;
int snd_count = 0;

//mutexes
pthread_mutex_t mtx_rcv;
pthread_mutex_t mtx_send;

//variaveis de condição
pthread_cond_t rcv_empty;
pthread_cond_t rcv_full;
pthread_cond_t snd_empty;
pthread_cond_t snd_full;

void init(void);
void *receiveClock(void *args); //função thread 1
void *updateClock(void *args); //função thread 2
void *sendClock(void *args); //função thread 3

int main() {
    int my_rank;
    
    MPI_Init(NULL, NULL);
    init();
    
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    //inicializa mutexes
    pthread_mutex_init(&mtx_rcv, NULL);
    pthread_mutex_init(&mtx_send, NULL);
    
    //inicializa variaveis de condição
    pthread_cond_init(&rcv_empty, NULL);
    pthread_cond_init(&rcv_full, NULL);
    pthread_cond_init(&snd_empty, NULL);
    pthread_cond_init(&snd_full, NULL);
    
    pthread_t thread1, thread2, thread3;
 
    pthread_create(&thread1, NULL, receiveClock, (void *)my_rank);
    pthread_create(&thread2, NULL, updateClock, (void *)my_rank);
    pthread_create(&thread3, NULL, sendClock, (void *)my_rank);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    
    //destroi mutexes
    pthread_mutex_destroy(&mtx_rcv);
    pthread_mutex_destroy(&mtx_send);
    
    //destroi variaveis de condição
    pthread_cond_destroy(&rcv_empty);
    pthread_cond_destroy(&rcv_full);
    pthread_cond_destroy(&snd_empty);
    pthread_cond_destroy(&snd_full);
    
    MPI_Finalize(); 

    return 0;
}

void init() {
    srand(time(NULL));
    Clock c = {
        .p[0] = rand() % 100,
        .p[1] = rand() % 100,
        .p[2] = rand() % 100
    };
    rcv[0] = c;
    rcv_count++;
}

/* -----------------------------------------------------------------------------*/

void* receiveClock(void* args) {
    long my_rank = (long) args;
    int rcv_id = (my_rank == 0)? 2 :(my_rank==1)? 0 : 1;
    
    while(1) {
    
        pthread_mutex_lock(&mtx_rcv);//bloqueia mutex da fila de recebimento
    
        while (rcv_count == BUFFER_SIZE){
            //printf("O BUFFER DE RECEBIMENTO ESTA CHEIO\n");
            pthread_cond_wait(&rcv_full, &mtx_rcv); // Libera mutex mtx_rcv e fica bloqueado na variável rcv_full
        }
   
        MPI_Recv(&rcv[rcv_count], sizeof(Clock), MPI_BYTE, rcv_id, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        rcv_count++;
        printf("processo %d recebeu relógio de %d\n",(int) my_rank, rcv_id);
    
        pthread_mutex_unlock(&mtx_rcv);//desbloqueia mutex da fila de recebimento
        pthread_cond_signal(&rcv_empty);//desbloqueia thread 2
    }
    
    return NULL;
}

void* updateClock(void* args) {
    long my_rank = (long) args;
    
    while(1) {
    
        pthread_mutex_lock(&mtx_rcv);//bloqueia fila de envio

    
        while (rcv_count == 0){
            //printf("O BUFFER DE RECEBIMENTO ESTA VAZIO\n");
            pthread_cond_wait(&rcv_empty, &mtx_rcv); // Libera mutex mtx_rcv e fica bloqueado na variável rcv_empty
        }
   
        Clock clock = rcv[0];
        for (int i=0; i<rcv_count-1; i++) {
            rcv[i] = rcv[i+1];
        }
        rcv_count--;
    
        //evento e compara relogios
        printf("processo %d atualizou relógio (%d,%d,%d)\n", (int) my_rank, clock.p[0],clock.p[1],clock.p[2]);
    
        pthread_mutex_unlock(&mtx_rcv);//desbloqueia fila de recebimento
        pthread_cond_signal(&rcv_full); //desbloqueia thread 1
    
        pthread_mutex_lock(&mtx_send); //bloqueia fila de envio
    
        while (snd_count == BUFFER_SIZE){
            //printf("O BUFFER DE ENVIO ESTA CHEIO\n");
            pthread_cond_wait(&snd_full, &mtx_send); // Libera mutex mtx_send e fica bloqueado na variável snd_full
        }
    
        snd[snd_count] = clock;
        snd_count++;
    
        pthread_mutex_unlock(&mtx_send);//libera fila de envio
        pthread_cond_signal(&snd_empty); //desbloqueia thread 3

    }
    
    return NULL;
}

void* sendClock(void* args) {
    long my_rank = (long) args;
    int send_id = (my_rank == 0)? 1 :(my_rank==1)? 2 : 0;
    
    while(1) {
    
        pthread_mutex_lock(&mtx_send);//bloqueia mutex da fila de envio

        while (snd_count == 0){
            //printf("O BUFFER DE ENVIO ESTA VAZIO\n");
            pthread_cond_wait(&snd_empty, &mtx_send); // Libera mutex mtx_send e fica bloqueado na variável snd_empty
        }
   
        Clock clock = snd[0];
        for (int i = 0; i < snd_count - 1; i++) {
            snd[i] = snd[i + 1];
        }
        snd_count--;
        printf("processo %d enviando relógio (%d,%d,%d)\n", (int)my_rank, clock.p[0],clock.p[1],clock.p[2]);

    
        pthread_mutex_unlock(&mtx_send); //desbloqueia mutex da fila de envio
        pthread_cond_signal(&snd_full); //desbloqueia thread 2
    
        MPI_Send(&clock, sizeof(Clock), MPI_BYTE, send_id, 0, MPI_COMM_WORLD);
    }
    return NULL;
}
