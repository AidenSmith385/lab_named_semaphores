/*-------------------------------------------------------------------------------
Demo: IPC using Shared Memory
Written By: 
     1- Dr. Mohamed Aboutabl
     2- Aiden Smith
     3- Braden Drake
     
     The P1 Process:    p1.c
     
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

// Named semaphore identifiers
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

    // Unlink the named semaphores
    Sem_unlink(SEM_MUTEX);
    Sem_unlink(SEM_P1_START);
    Sem_unlink(SEM_P1_DONE);
    Sem_unlink(SEM_P2_START);
    Sem_unlink(SEM_P2_DONE);

    // Detach and remove the shared memory
    Shmdt(p);
    shmctl(shmid, IPC_RMID, NULL);
}

int main(int argc, char *argv[]) {

    // Signal handlers for Ctrl-C and TERM
    sigactionWrapper(SIGINT,  cleanup);
    sigactionWrapper(SIGTERM, cleanup);

    // Create and attach shared memory
    key_t shmkey = ftok("shmSegment.h", 5);
    int   shmflg = IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR;
    shmid = shmget(shmkey, SHMEM_SIZE, shmflg);
    if (shmid == -1) {
        printf("\nP1 Failed to get shared memory id=%d\n", shmid);
        perror("Reason");
        exit(-1);
    }
    p = (shmData *) shmat(shmid, NULL, 0);
    if (p == (shmData *) -1) {
        printf("\nP1 Failed to attach shared memory id=%d\n", shmid);
        perror("Reason");
        exit(-1);
    }

    // Semaphore flags and modes
    int semflg = O_CREAT | O_EXCL;
    int semmode = S_IRUSR | S_IWUSR;

    // Create named semaphores
    mutex = Sem_open(SEM_MUTEX, semflg, semmode, 1);
    p1_start = Sem_open(SEM_P1_START, semflg, semmode, 0);
    p1_done = Sem_open(SEM_P1_DONE, semflg, semmode, 0);
    p2_start = Sem_open(SEM_P2_START, semflg, semmode, 0);
    p2_done = Sem_open(SEM_P2_DONE, semflg, semmode, 0);

    // Initialize shared data 
    p->counter   = 0;
    p->p1Done    = 0;
    p->p1Started = 1;

    // P1 is ready
    printf("P1 started. MANY = %10lu\n", MANY);
    Sem_post(p1_start);

    // Wait for P2
    printf("Waiting for P2 to start, too.\n");
    Sem_wait(p2_start);

    // Mutual exclusion, enter, increment, and then leave
    printf("P1 now will increment the counter\n");
    for (unsigned i = 1; i <= MANY; i++) {
        Sem_wait(mutex);
        p->counter++;
        Sem_post(mutex);
    }

    // Signal P2 that P1 is done
    p->p1Done = 1;
    Sem_post(p1_done);

    // Wait for P2
    printf("P1 is done. Waiting for P2 to finish, too.\n");
    Sem_wait(p2_done);

    unsigned long expected = MANY << 1; // 2 * MANY
    printf("P1 reports final counter value = %10u Expecting: %10lu",
           p->counter, expected);
    if (p->counter == expected) printf(" CORRECT\n"); else printf(" NOT CORRECT\n");

    // Cleanup
    cleanup(0);
    return 0;
}