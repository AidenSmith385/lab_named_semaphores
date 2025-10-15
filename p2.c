/*-------------------------------------------------------------------------------
Demo: IPC using Shared Memory
Written By: 
     1- Dr. Mohamed Aboutabl
     2- Aiden Smith
     3- Braden Drake
     
     The P2 Process:    p2.c
     
-------------------------------------------------------------------------------*/

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>

#include "shmSegment.h"
#include "wrappers.h"

// Named semaphore identifiers (must match P1)
#define SEM_MUTEX       "/team25_mutex"
#define SEM_P1_START    "/team25_p1_start"
#define SEM_P1_DONE     "/team25_p1_done"
#define SEM_P2_START    "/team25_p2_start"
#define SEM_P2_DONE     "/team25_p2_done"

sem_t *mutex, *p1_start, *p1_done, *p2_start, *p2_done; // Semaphores
int shmid;
shmData *p; // Pointer to shared memory

// Cleanup helper function
static void cleanup(int sig) {
    // Close the semaphores
    Sem_close(mutex);
    Sem_close(p1_start);
    Sem_close(p1_done);
    Sem_close(p2_start);
    Sem_close(p2_done);

    // Detach shared memory
    Shmdt(p);
}

int main(int argc, char *argv[]) {

    // Signal handlers for Ctrl-C and TERM 
    sigactionWrapper(SIGINT,  cleanup);
    sigactionWrapper(SIGTERM, cleanup);

    // Attach to p1 shared memory
    key_t shmkey = ftok("shmSegment.h", 5);
    int   shmflg = S_IRUSR | S_IWUSR; // No IPC_CREAT
    shmid = shmget(shmkey, SHMEM_SIZE, shmflg);
    if (shmid == -1) {
        printf("\nP2 Failed to get shared memory id=%d\n", shmid);
        perror("Reason");
        exit(-1);
    }
    p = (shmData *) shmat(shmid, NULL, 0);
    if (p == (shmData *) -1) {
        printf("\nP2 Failed to attach shared memory id=%d\n", shmid);
        perror("Reason");
        exit(-1);
    }

    // Semaphore flag
    int semflg = O_RDWR;

    // Semaphores created by P1
    mutex = Sem_open2(SEM_MUTEX, semflg);
    p1_start = Sem_open2(SEM_P1_START, semflg);
    p1_done  = Sem_open2(SEM_P1_DONE, semflg);
    p2_start = Sem_open2(SEM_P2_START, semflg);
    p2_done = Sem_open2(SEM_P2_DONE, semflg);

    // Initialize P2 shared data
    p->p2Started = 1;
    p->p2Done = 0;

    // Tell P1 we’re ready
    printf("P2 started. MANY = %10lu\n", MANY);
    Sem_post(p2_start);

    // Wait for P1
    printf("Waiting for P1 to start, too.\n");
    Sem_wait(p1_start);

    // Mutual exclusion, enter, increment, and then leave
    printf("P2 now will increment the counter\n");
    for (unsigned i = 1; i <= MANY; i++) {
        Sem_wait(mutex);
        p->counter++;
        Sem_post(mutex);
    }

    // Signal P1 that P2 is done
    p->p2Done = 1;
    Sem_post(p2_done);

    // Wait for P1
    printf("P2 is done. Waiting for P1 to finish, too.\n");
    Sem_wait(p1_done);

    unsigned long expected = MANY << 1; // 2 * MANY
    printf("P2 reports final counter value = %10u Expecting: %10lu",
           p->counter, expected);
    if (p->counter == expected) printf(" CORRECT\n"); else printf(" NOT CORRECT\n");

    // Cleanup
    cleanup(0);
    return 0;
}