/*-------------------------------------------------------------------------------
IPC using Shared Memory
Written By: 
     1- Dr. Mohamed Aboutabl
-------------------------------------------------------------------------------*/

#ifndef SHMEM_SEGMENT
#define SHMEM_SEGMENT


typedef struct {
    int         p1Started , p2Started ;
    int         p1Done    , p2Done ;
    unsigned    counter ;
} shmData ;

#define SHMEM_SIZE sizeof(shmData)
#define MANY   1000L

#endif
