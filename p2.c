/*-------------------------------------------------------------------------------
Demo: IPC using Shared Memory
Written By: 
     1- Dr. Mohamed Aboutabl
     2- Aiden Smith
     3- Braden Drake
     
     The P2 Process:    p2.c
     
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

// Named semaphore identifiers (must match P1)
#define SEM_MUTEX_NAME "/lab05_mutex"
#define SEM_START_NAME "/lab05_start"
#define SEM_DONE_NAME  "/lab05_done"

static sem_t *g_mutex = NULL;
static sem_t *g_start = NULL;
static sem_t *g_done  = NULL;

static int     g_shmid = -1;
static shmData *g_p    = NULL;

static volatile sig_atomic_t g_shutdown = 0;

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
            if (g_shutdown) return -1;
            continue;
        }
        return -1;
    }
}

static void cleanup(void) {
    if (g_p) {
        Shmdt(g_p);
        g_p = NULL;
    }
    if (g_mutex) { Sem_close(g_mutex); g_mutex = NULL; }
    if (g_start) { Sem_close(g_start); g_start = NULL; }
    if (g_done)  { Sem_close(g_done);  g_done  = NULL; }
    // does NOT unlink semaphores and does NOT remove SHM
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    // Install signal handlers 
    sigactionWrapper(SIGINT,  on_signal);
    sigactionWrapper(SIGTERM, on_signal);

    // Attach to p1 shared memory
    key_t shmkey = ftok("shmSegment.h", 5);
    int   shmflg = S_IRUSR | S_IWUSR; // No IPC_CREAT
    g_shmid = shmget(shmkey, SHMEM_SIZE, shmflg);
    if (g_shmid == -1) {
        printf("\nP2 Failed to get shared memory id=%d\n", g_shmid);
        perror("Reason");
        exit(-1);
    }
    g_p = (shmData *) shmat(g_shmid, NULL, 0);
    if (g_p == (shmData *) -1) {
        printf("\nP2 Failed to attach shared memory id=%d\n", g_shmid);
        perror("Reason");
        exit(-1);
    }

    // semaphores created by P1
    g_mutex = Sem_open2(SEM_MUTEX_NAME, 0);
    g_start = Sem_open2(SEM_START_NAME, 0);
    g_done  = Sem_open2(SEM_DONE_NAME,  0);

    g_p->p2Done    = 0;
    g_p->p2Started = 1;

    printf("P2 started. MANY = %10lu\n", MANY);
    printf("Announcing P2 is ready (post /lab05_start), then incrementing with mutex.\n");

    // tell P1 we’re ready
    Sem_post(g_start);

    if (g_shutdown) { cleanup(); return 1; }

    for (unsigned i = 1; i <= MANY; i++) {
        if (g_shutdown) break;
        if (robust_sem_wait(g_mutex) != 0) {
            fprintf(stderr, "P2: interrupted waiting for mutex; breaking.\n");
            break;
        }
        g_p->counter++;
        Sem_post(g_mutex);
    }

    g_p->p2Done = 1;

    printf("P2 is done. Waiting for P1 to finish, too (wait on /lab05_done).\n");

    // Wait for P1 to announce it’s done
    if (robust_sem_wait(g_done) != 0) {
        fprintf(stderr, "P2: interrupted while waiting on done; cleaning up.\n");
        cleanup();
        return 1;
    }

    unsigned long expected = MANY << 1; // 2 * MANY
    printf("P2 reports final counter value = %10u Expecting: %10lu",
           g_p->counter, expected);
    if (g_p->counter == expected) printf(" CORRECT\n"); else printf(" NOT CORRECT\n");

    cleanup();
    return 0;
}
