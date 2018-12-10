#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "pv.h"

int 
init_pv()
{
    int            i, ret;
    int            semid;
    union semun {
        int val ;
        struct semid_ds *buf ;
        unsigned short *array ;
        struct seminfo *_buf ;
    } arg;

    unsigned short buf[MAX_SEMAPHORE] ;

    for(i = 0; i < MAX_SEMAPHORE; ++i) {
         /* Initial semaphore */
         buf[i] = i + 1;
    }
    
    semid = semget(IPC_PRIVATE, MAX_SEMAPHORE, IPC_CREAT|0666);
    if (semid == -1) {
        fprintf(stderr, "Error in semget:%s\n", strerror(errno));
        exit(-1) ;
    }

    arg.array = buf;
    ret = semctl(semid , 0, SETALL, arg);
    if (ret == -1) {
         fprintf(stderr, "Error in semctl:%s!\n", strerror(errno));
         exit(-1) ;
    }
    return semid;
}

//p
int
P(int semid)
{
    int    i;
    struct sembuf  sb[MAX_SEMAPHORE];

    for(i = 0; i < MAX_SEMAPHORE; ++i) {
        sb[i].sem_num = i ;
        sb[i].sem_op = -1 ;
        sb[i].sem_flg = 0 ;
    }

    return semop(semid , sb , 10);
}

//v
int
V(int semid)
{
    int i;
    struct sembuf  sb[MAX_SEMAPHORE];

    for(i = 0; i < MAX_SEMAPHORE; ++i) {
        sb[i].sem_num = i ;
        sb[i].sem_op = +1 ;
        sb[i].sem_flg = 0 ;
    }

    return semop(semid , sb , 10);
}


void
rm_pv(int sem_id)
{
    if (semctl(sem_id, 0, IPC_RMID, 0) == -1) {
        fprintf(stderr, "Error in semctl:%s!\n", strerror(errno));
    }
}
