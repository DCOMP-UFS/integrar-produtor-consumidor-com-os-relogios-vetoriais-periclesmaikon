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
   int receiver;
} Clock;

//filas
Clock rcv[BUFFER_SIZE];
Clock snd[BUFFER_SIZE];

//contadores
int rcv_count = 0;
int snd_count = 0;

//mutexes
pthread_mutex_t mtx_rcv = PTHREAD_MUTEX_INITIALIZER;;
pthread_mutex_t mtx_send = PTHREAD_MUTEX_INITIALIZER;;
pthread_mutex_t mtx_clock = PTHREAD_MUTEX_INITIALIZER;;

//variaveis de condição
pthread_cond_t rcv_empty;
pthread_cond_t rcv_full;
pthread_cond_t snd_empty;
pthread_cond_t snd_full;

//funções das threads
void *receiveClock(void *args); //função thread 1
void *mainThread(void *args); //função thread 2
void *sendClock(void *args); //função thread 3

Clock my_clock = {{0, 0, 0}, -1};

int main() {
    int my_rank;
    
    MPI_Init(NULL, NULL);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    //inicializa variaveis de condição
    pthread_cond_init(&rcv_empty, NULL);
    pthread_cond_init(&rcv_full, NULL);
    pthread_cond_init(&snd_empty, NULL);
    pthread_cond_init(&snd_full, NULL);
  
    pthread_t thread1, thread2, thread3;
 
    pthread_create(&thread1, NULL, receiveClock, (void *)(long)my_rank);
    pthread_create(&thread2, NULL, mainThread, (void *)(long)my_rank);
    pthread_create(&thread3, NULL, sendClock, (void *)(long)my_rank);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    
    //destroi variaveis de condição
    pthread_cond_destroy(&rcv_empty);
    pthread_cond_destroy(&rcv_full);
    pthread_cond_destroy(&snd_empty);
    pthread_cond_destroy(&snd_full);
    
    MPI_Finalize(); 

    return 0;
}

/* -----------------------------------------------------------------------------*/

void event(int pid){
   pthread_mutex_lock(&mtx_clock);
   my_clock.p[pid]++;
   printf("Processo %d fez evento: (%d, %d, %d)\n", pid, my_clock.p[0], my_clock.p[1], my_clock.p[2]);
   pthread_mutex_unlock(&mtx_clock);
}

void receive(int pid) {
    
    pthread_mutex_lock(&mtx_rcv);//bloqueia fila de recebimento

    while (rcv_count == 0){
        printf("O BUFFER DE RECEBIMENTO DE %d ESTA VAZIO\n", pid);
        pthread_cond_wait(&rcv_empty, &mtx_rcv); // Libera mutex mtx_rcv e fica bloqueado na variável rcv_empty
    }
    
    Clock clock = rcv[0];
    for (int i=0; i<rcv_count-1; i++) {
        rcv[i] = rcv[i+1];
    }
    rcv_count--;
    pthread_mutex_unlock(&mtx_rcv);//desbloqueia fila de recebimento
    pthread_cond_signal(&rcv_full); //desbloqueia thread 1
    
    pthread_mutex_lock(&mtx_clock);
    my_clock.p[pid]++;
    // Atualizar o relógio local para ser o máximo entre o atual e o recebido
    for (int i = 0; i < 3; i++) {
    
        if (my_clock.p[i] < clock.p[i]) {
            my_clock.p[i] = clock.p[i];
        }
    }
    printf("Processo %d recebeu: (%d, %d, %d)\n", pid, my_clock.p[0], my_clock.p[1], my_clock.p[2]);
    pthread_mutex_unlock(&mtx_clock);
}

void send(int pid, int receiver) {
    
    pthread_mutex_lock(&mtx_clock);
    my_clock.p[pid]++;
    Clock temp = my_clock;
    pthread_mutex_unlock(&mtx_clock);
    
    pthread_mutex_lock(&mtx_send); //bloqueia fila de envio
    while (snd_count == BUFFER_SIZE){
        printf("O BUFFER DE ENVIO DE %d ESTA VAZIO\n", pid);
        pthread_cond_wait(&snd_full, &mtx_send); // Libera mutex mtx_send e fica bloqueado na variável snd_full
    }
    
    temp.receiver = receiver;
    snd[snd_count] = temp;
    snd_count++;
    printf("Processo %d enviou (%d, %d, %d) para o processo %d\n", pid, my_clock.p[0], my_clock.p[1], my_clock.p[2], receiver);
    pthread_mutex_unlock(&mtx_send);//libera fila de envio
    pthread_cond_signal(&snd_empty); //desbloqueia thread 3
}

/* -----------------------------------------------------------------------------*/

void* mainThread(void* args) {
    long my_rank = (long) args;
    int finalizado = 0;
    
    if(finalizado == 2){
        pthread_mutex_unlock(&mtx_rcv);
        pthread_cond_signal(&rcv_empty);
        
        pthread_cond_signal(&snd_full);
    }

    if (my_rank == 0) {
        event(0);
        send(0,1);
        receive(0);
        send(0,2);
        receive(0);
        send(0,1);
        event(0);
        printf("%d -> (%d,%d,%d)\n",0,my_clock.p[0], my_clock.p[1],my_clock.p[2]);
        finalizado++;

    } else if (my_rank == 1) {
        send(1,0);
        receive(1);
        receive(1);
        printf("%d -> (%d,%d,%d)\n",1,my_clock.p[0], my_clock.p[1],my_clock.p[2]);
        finalizado++;

    } else if (my_rank == 2) {
        event(2);
        send(2,0);
        receive(2);
        printf("%d -> (%d,%d,%d)\n",2,my_clock.p[0], my_clock.p[1],my_clock.p[2]);
        finalizado++;
    }
    return NULL;
}

void* receiveClock(void* args) {
    long my_rank = (long) args;
    
   while(1) {
    
        pthread_mutex_lock(&mtx_rcv);//bloqueia mutex da fila de recebimento
    
        while (rcv_count == BUFFER_SIZE){
            //printf("O BUFFER DE RECEBIMENTO ESTA CHEIO\n");
            pthread_cond_wait(&rcv_full, &mtx_rcv); // Libera mutex mtx_rcv e fica bloqueado na variável rcv_full
        }
        
        MPI_Status status;
        MPI_Recv(&rcv[rcv_count], sizeof(struct Clock), MPI_BYTE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        Clock c = rcv[rcv_count];
        rcv_count++;
        //printf("processo %d recebeu relógio (%d,%d,%d) do processo %d\n",(int) my_rank,c.p[0],c.p[1],c.p[2], status.MPI_SOURCE);

        pthread_mutex_unlock(&mtx_rcv);//desbloqueia mutex da fila de recebimento
        pthread_cond_signal(&rcv_empty);//desbloqueia thread 2
   }
    
    return NULL;
}

void* sendClock(void* args) {
    long my_rank = (long) args;
    
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
        //printf("processo %d enviando relógio (%d,%d,%d) para %d\n", (int)my_rank, clock.p[0],clock.p[1],clock.p[2], clock.receiver);

    
        pthread_mutex_unlock(&mtx_send); //desbloqueia mutex da fila de envio
        pthread_cond_signal(&snd_full); //desbloqueia thread 2
    
        MPI_Send(&clock, sizeof(Clock), MPI_BYTE, clock.receiver, 0, MPI_COMM_WORLD);
    }
    return NULL;
}
