/* Parte 3 - PPC
    Implementação do modelo produtor consumidor utilizando threads e MPI
    compilar: mpicc -o part3 part3.c -lpthread
    executar: mpiexec -n 3 ./part3
*/
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <pthread.h> 
#include <unistd.h>
#include <semaphore.h>
#include <time.h>


#define BUFFER_SIZE 4 //tamanho das filas

typedef struct Clock {
    int p[3];
    int receiver;
} Clock;

//filas
Clock rcv[BUFFER_SIZE];
Clock snd[BUFFER_SIZE];

//contadores de relogios das filas
int rcv_count = 0;
int snd_count = 0;

//mutexes 
pthread_mutex_t mtx_rcv = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx_snd = PTHREAD_MUTEX_INITIALIZER;

//variáveis de condição
pthread_cond_t rcv_empty;
pthread_cond_t rcv_full;
pthread_cond_t snd_empty;
pthread_cond_t snd_full;

//funções das threads
void* receivingThread(void* args);
void* mainThread(void* args);
void* sendingThread(void* args);

//clock do processo
Clock my_clock = {{0, 0, 0}, -1};

//processo mostra seu próprio relógio (debug)
void printMyClock(int pid) {
    printf("%d: my clock is (%d,%d,%d)\n", pid, my_clock.p[0], my_clock.p[1], my_clock.p[2]);

}
//evento do processo pid
void event(int pid) {
    my_clock.p[pid]++;
}

//salva relógio recebido como parametro na fila de recebidos
void queueRcv(Clock clock) {
    pthread_mutex_lock(&mtx_rcv);
    
    while (rcv_count == BUFFER_SIZE) {
        printf("Buffer de recebimento cheio!\n");
        pthread_cond_wait(&rcv_full, &mtx_rcv);
    }
    
    rcv[rcv_count] = clock;
    rcv_count++;
    
    pthread_mutex_unlock(&mtx_rcv);
    pthread_cond_signal(&rcv_empty);
    
}

//pega relogio na fila de recebidos e compara com relogio interno do processo
void compare(int pid) {
    pthread_mutex_lock(&mtx_rcv);
    
    while (rcv_count == 0) {
        pthread_cond_wait(&rcv_empty, &mtx_rcv);
    }
    
    Clock newClock = rcv[0];
    int i;
    for (i = 0; i < rcv_count-1; i++) {
        rcv[i] = rcv[i+1];
    }
    rcv_count--;
    
    pthread_mutex_unlock(&mtx_rcv);
    pthread_cond_signal(&rcv_full);
    
    for (i=0; i<3; i++) {
        if (my_clock.p[i] < newClock.p[i]) {
            my_clock.p[i] = newClock.p[i];
        }
    }
    
    my_clock.p[pid]++;
}

//salva relogio do processo na fila de envio
void queueSnd(int pid, int receiver) {
    my_clock.p[pid]++;
    
    pthread_mutex_lock(&mtx_snd);
    
    while (snd_count == BUFFER_SIZE) {
        printf("Buffer de envio cheio!\n");
        pthread_cond_wait(&snd_full, &mtx_snd);
    }
    
    Clock temp = my_clock;
    temp.receiver = receiver;
    snd[snd_count] = temp;
    snd_count++;
    
    pthread_mutex_unlock(&mtx_snd);
    pthread_cond_signal(&snd_empty);
}

//retorna um relogio da fila de envio
Clock send() {
    pthread_mutex_lock(&mtx_snd);
    
    while(snd_count == 0) {
        pthread_cond_wait(&snd_empty, &mtx_snd);
    }
    
    Clock clock = snd[0];
    for (int i=0; i<snd_count-1; i++) {
        snd[i] = snd[i+1];
    }
    snd_count--;
    
    pthread_mutex_unlock(&mtx_snd);
    pthread_cond_signal(&snd_full);
    
    return clock;
}

int main() {
    int my_rank;
    
    MPI_Init(NULL, NULL);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    //inicializa variáveis de condição
    pthread_cond_init(&rcv_empty, NULL);
    pthread_cond_init(&rcv_full, NULL);
    pthread_cond_init(&snd_empty, NULL);
    pthread_cond_init(&snd_full, NULL);
    
    pthread_t thread1, thread2, thread3;
    
    // inicializa thread de entrada
    if (pthread_create(&thread1, NULL, &receivingThread, (void*)(long)my_rank) != 0) {
         perror("Failed to create thread 1");
    }
    
    // inicializa thread principal
    if (pthread_create(&thread2, NULL, &mainThread, (void*)(long)my_rank) != 0) {
         perror("Failed to create thread 2");
    } 
    
    // inicializa thread de saída
    if (pthread_create(&thread3, NULL, &sendingThread, (void*)(long)my_rank) != 0) {
         perror("Failed to create thread 3");
    }
    
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


/*---------------------------------------------------------*/

//recebe relogios de outros processos
void* receivingThread(void* args) {
    int my_rank = (int)((long)args);
    Clock clock;

    while (1) {
        MPI_Recv(&clock, sizeof(Clock), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        queueRcv(clock);
    }

    return NULL;
}

//gerencia o recebimento, comparação e envio de relogios
void* mainThread(void* args) {
    int my_rank = (int)((long)args);
    
    if (my_rank == 0) {
        event(0);
        queueSnd(0, 1);
        compare(0);
        queueSnd(0,2);
        compare(0);
        queueSnd(0, 1);
        event(0);
        printMyClock(0);
    } else if (my_rank == 1) {
        queueSnd(1,0);
        compare(1);
        compare(1);
        printMyClock(1);
    } else if (my_rank == 2) {
        event(2);
        queueSnd(2,0);
        compare(2);
        printMyClock(2);
    }

    return NULL;
}

//envia relogios que estiverem na fila de envio
void* sendingThread(void* args) {
    int my_rank = (int)((long)args);
    Clock clock;
    int receiver;

    while (1) {
        clock = send();
        receiver = clock.receiver;
        MPI_Send(&clock, sizeof(Clock), MPI_BYTE, receiver, 0, MPI_COMM_WORLD);
        printf("%d sent clock (%d,%d,%d) to %d\n", my_rank, clock.p[0], clock.p[1], clock.p[2], receiver);
    }

    return NULL;
}
