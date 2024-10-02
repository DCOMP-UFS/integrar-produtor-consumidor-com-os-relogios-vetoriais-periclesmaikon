//Compilação: mpicc -o integracao integracao.c -pthread
//Execução:   mpiexec -n 3 ./integracao

#include <stdio.h>
#include <mpi.h> 
#include <pthread.h> 
#include <semaphore.h>

#define BUFFER_SIZE 8   // Número máximo de relógios enfileirados

//Definicao do relogio
typedef struct Clock { 
   int p[3];
} Clock;

Clock clockQueue[BUFFER_SIZE];
int clockCount = 0;

// Variáveis globais de sincronização
pthread_mutex_t mutex;
pthread_cond_t condFull;
pthread_cond_t condEmpty;

//----------------------------------------------------------//

//Adiciona relogio no buffer
void submitClock(Clock clock){
   pthread_mutex_lock(&mutex);

   while (clockCount == BUFFER_SIZE){
      printf("O BUFFER ESTA CHEIO\n");
      pthread_cond_wait(&condFull, &mutex); // Libera mutex e fica bloqueado na variável condFull
   }
   printf("Salvando Relogio\n");
   clockQueue[clockCount] = clock; // Coloca um relógio no buffer
   clockCount++;
   
   pthread_mutex_unlock(&mutex); // Libera o mutex
   pthread_cond_signal(&condEmpty); // Desbloqueia uma thread consumidora
}

// Consome relógio respeitando a região crítica
Clock getClock(){
   pthread_mutex_lock(&mutex);
   
   while (clockCount == 0){
      printf("O BUFFER ESTA VAZIO\n");
      pthread_cond_wait(&condEmpty, &mutex); // Libera mutex e fica bloqueado na variável condEmpty
   }
   
   Clock clock = clockQueue[0];
   int i;
   for (i = 0; i < clockCount - 1; i++){
      clockQueue[i] = clockQueue[i + 1]; // Consome o primeiro valor da fila e move os outros
   }
   clockCount--; // Diminui o contador de relógios
   
   pthread_mutex_unlock(&mutex); // Libera o mutex
   pthread_cond_signal(&condFull); // Desbloqueia uma thread produtora presa na variável condFull
   return clock;
}


//----------------------------------------------------------//


//Funcao de evento
void Event(int pid, Clock *clock){
    clock->p[pid]++;
    submitClock(*clock);
}

//Funcao de envio
void Send(int pid, Clock *clock, int pidDestino){
    clock->p[pid]++;
    MPI_Send(clock, sizeof(Clock), MPI_BYTE, pidDestino, 0, MPI_COMM_WORLD);
    submitClock(*clock);
}

//Funcao para receber
void Receive(int pid, Clock *clock, int pidOrigem){
    Clock temp;
    MPI_Recv(&temp, sizeof(Clock), MPI_BYTE, pidOrigem, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    clock->p[pid]++;
    // Atualiza o relógio local para ser o máximo entre o atual e o recebido
    for (int i = 0; i < 3; i++) {
        if (clock->p[i] < temp.p[i]) {
            clock->p[i] = temp.p[i];
       }
   }
   submitClock(*clock);
}


//----------------------------------------------------------//


//Representa processo 0
void Processo0(){
   Clock clock = {{0,0,0}};
   int pid = 0, pidDestino, pidOrigem;
   
   //Event
   pthread_join(t3, NULL);
   printf("Process: %d, ClockA: (%d, %d, %d)\n", 0, clock.p[0], clock.p[1], clock.p[2]);
   
   //Send
   pidDestino = 1;
   pthread_join(t1, NULL);
   printf("Process: %d, ClockB: (%d, %d, %d)\n", 0, clock.p[0], clock.p[1], clock.p[2]);
   
   //Receive
   pidOrigem = 1;
   pthread_join(t2, NULL);
   printf("Process: %d, ClockC: (%d, %d, %d)\n", 0, clock.p[0], clock.p[1], clock.p[2]);
   
   //Send
   pidDestino = 2;
   pthread_join(t1, NULL);
   printf("Process: %d, ClockD: (%d, %d, %d)\n", 0, clock.p[0], clock.p[1], clock.p[2]);
   
   //Receive
   pidOrigem = 2;
   pthread_join(t2, NULL);
   printf("Process: %d, ClockE: (%d, %d, %d)\n", 0, clock.p[0], clock.p[1], clock.p[2]);
   
   //Send
   pidDestino = 1;
   pthread_join(t1, NULL);
   printf("Process: %d, ClockF: (%d, %d, %d)\n", 0, clock.p[0], clock.p[1], clock.p[2]);
   
   //Event
   pthread_join(t3, NULL);
   printf("Process: %d, ClockG: (%d, %d, %d)\n", 0, clock.p[0], clock.p[1], clock.p[2]);
}

//Representa processo 1
void Processo1(){
   Clock clock = {{0,0,0}};
   int pid = 1, pidDestino, pidOrigem;
   
   //Send
   pidDestino = 0;
   pthread_join(t1, NULL);
   printf("Process: %d, ClockH: (%d, %d, %d)\n", 1, clock.p[0], clock.p[1], clock.p[2]);
   
   //Receive
   pidOrigem = 0;
   pthread_join(t2, NULL);
   printf("Process: %d, ClockI: (%d, %d, %d)\n", 1, clock.p[0], clock.p[1], clock.p[2]);
   
   //Receive
   pidOrigem = 0;
   pthread_join(t2, NULL);
   printf("Process: %d, ClockJ: (%d, %d, %d)\n", 1, clock.p[0], clock.p[1], clock.p[2]);
}

//Representa processo 2
void Processo2(){
   Clock clock = {{0,0,0}};
   int pid = 2, pidDestino, pidOrigem;
   
   //Event
   pthread_join(t3, NULL);
   printf("Process: %d, ClockK: (%d, %d, %d)\n", 2, clock.p[0], clock.p[1], clock.p[2]);
   
   //Send
   pidDestino = 0;
   pthread_join(t1, NULL);
   printf("Process: %d, ClockL: (%d, %d, %d)\n", 2, clock.p[0], clock.p[1], clock.p[2]);
   
   //Receive
   pidOrigem = 0;
   pthread_join(t2, NULL);
   printf("Process: %d, ClockM: (%d, %d, %d)\n", 2, clock.p[0], clock.p[1], clock.p[2]);
}


//----------------------------------------------------------//


int main(void) {
   int my_rank;
   long i = 1;
   pthread_t t1, t2, t3;

   MPI_Init(NULL, NULL); //Inicia MPI
   MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
   
   //Inicia Variáveis de sincronização
   pthread_mutex_init(&mutex, NULL);
   pthread_cond_init(&condEmpty, NULL);
   pthread_cond_init(&condFull, NULL);
   
   //Criação das threads
   pthread_create(&t1, NULL, Send(int pid, Clock *clock, int pidDestino), (void*) i);
   pthread_create(&t2, NULL, Receive(int pid, Clock *clock, int pidOrigem), (void*) i);
   pthread_create(&t3, NULL, Event(int pid, Clock *clock), (void*) i);

   if (my_rank == 0) { 
      Processo0();
   } else if (my_rank == 1) {  
      Processo1();
   } else if (my_rank == 2) {  
      Processo2();
   }


   MPI_Finalize(); //Finaliza MPI

   return 0;
}
