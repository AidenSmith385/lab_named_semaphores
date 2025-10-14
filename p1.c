/*-------------------------------------------------------------------------------
Demo: IPC using Shared Memory
Written By: 
     1- Dr. Mohamed Aboutabl
     2- Aiden Smith
     3- Braden Drake
     
     The P1 Process:    p1.c
     
-------------------------------------------------------------------------------*/

#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <semaphore.h>
#include <string.h>

#include "shmSegment.h"
#include "wrappers.h"

// Named semaphore identifiers
#define SEM_MUTEX_NAME "/lab05_mutex"
#define SEM_START_NAME "/lab05_start"
#define SEM_DONE_NAME  "/lab05_done"

static sem_t *g_mutex = NULL;
static sem_t *g_start = NULL;
static sem_t *g_done  = NULL;

static int     g_shmid = -1;
static shmData *g_p    = NULL;

static volatile sig_atomic_t g_shutdown = 0;
static const bool I_AM_CREATOR = true;

// Signal handling 
static void on_signal(int signo) {
    (void)signo;
    g_shutdown = 1;
}

// wait that breaks cleanly on signals
static int robust_sem_wait(sem_t *s) {
    while (1) {
        if (sem_wait(s) == 0) return 0;
        if (errno == EINTR) {
            if (g_shutdown) return -1; // caller will clean up
            continue; // retry
        }
        // fail
        return -1;
    }
}

static void cleanup(void) {
    if (g_p) {
        Shmdt(g_p);
        g_p = NULL;
    }
    if (g_shmid != -1) {
        // P1 removes shared memory
        shmctl(g_shmid, IPC_RMID, NULL);
        g_shmid = -1;
    }
    if (g_mutex) { Sem_close(g_mutex); g_mutex = NULL; }
    if (g_start) { Sem_close(g_start); g_start = NULL; }
    if (g_done)  { Sem_close(g_done);  g_done  = NULL; }

    // As creator, P1 unlinks named semaphores
    Sem_unlink(SEM_MUTEX_NAME);
    Sem_unlink(SEM_START_NAME);
    Sem_unlink(SEM_DONE_NAME);
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    // Install signal handlers
    sigactionWrapper(SIGINT,  on_signal);
    sigactionWrapper(SIGTERM, on_signal);

    // Create shared memory
    key_t shmkey = ftok("shmSegment.h", 5);
    int   shmflg = IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR;
    g_shmid = shmget(shmkey, SHMEM_SIZE, shmflg);
    if (g_shmid == -1) {
        printf("\nP1 Failed to get shared memory id=%d\n", g_shmid);
        perror("Reason");
        exit(-1);
    }
    g_p = (shmData *) shmat(g_shmid, NULL, 0);
    if (g_p == (shmData *) -1) {
        printf("\nP1 Failed to attach shared memory id=%d\n", g_shmid);
        perror("Reason");
        exit(-1);
    }

    // Initialize shared data 
    g_p->counter   = 0;
    g_p->p1Done    = 0;
    g_p->p1Started = 1;
    g_p->p2Started = 0;
    g_p->p2Done    = 0;

    // Create named semaphores
    g_mutex = Sem_open(SEM_MUTEX_NAME, O_CREAT | O_EXCL, 0600, 1); // binary mutex
    g_start = Sem_open(SEM_START_NAME, O_CREAT | O_EXCL, 0600, 0); // start gate
    g_done  = Sem_open(SEM_DONE_NAME,  O_CREAT | O_EXCL, 0600, 0); // done gate

    printf("P1 started. MANY = %10lu\n", MANY);
    printf("Waiting for P2 to start, too (rendezvous on /lab05_start).\n");

    // wait until P2 is ready
    if (robust_sem_wait(g_start) != 0) {
        fprintf(stderr, "P1: interrupted while waiting on start; cleaning up.\n");
        cleanup();
        return 1;
    }

    if (g_shutdown) { cleanup(); return 1; }

    printf("P1 now will increment the counter (with mutex inside loop)\n");
    for (unsigned i = 1; i <= MANY; i++) {
        if (g_shutdown) break;
        // semaphore mutex
        if (robust_sem_wait(g_mutex) != 0) {
            fprintf(stderr, "P1: interrupted waiting for mutex; breaking.\n");
            break;
        }
        g_p->counter++;
        Sem_post(g_mutex);
    }

    g_p->p1Done = 1;

    // Signal P2 that P1 is done
    Sem_post(g_done);

    printf("P1 is done. Waiting for P2 to finish, too (via semaphore /lab05_done already posted by P1 for P2).\n");

    unsigned long expected = MANY << 1; // 2 * MANY
    printf("P1 reports final counter value = %10u Expecting: %10lu",
           g_p->counter, expected);
    if (g_p->counter == expected) printf(" CORRECT\n"); else printf(" NOT CORRECT\n");

    cleanup();
    return 0;
}
