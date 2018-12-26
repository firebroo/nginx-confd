#include "confd_shmtx.h"

#if (USE_SYSV_SEM)

#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

confd_shmtx_t* 
init_lock()
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

    confd_shmtx_t* shmtx = (confd_shmtx_t*)malloc(sizeof(confd_shmtx_t));
    if (!shmtx) {
        printf("malloc() failed.\n");
        return NULL;
    }
    for(i = 0; i < MAX_SEMAPHORE; ++i) {
         /* Initial semaphore */
         buf[i] = i + 1;
    }
    
    semid = semget(IPC_PRIVATE, MAX_SEMAPHORE, IPC_CREAT|0666);
    if (semid == -1) {
        printf("semget() failed error: %s\n", strerror(errno));
        free(shmtx);
        return NULL;
    }

    arg.array = buf;
    ret = semctl(semid , 0, SETALL, arg);
    if (ret == -1) {
        printf("semctl() failed error: %s\n", strerror(errno));
        free(shmtx);
        return NULL;
    }
    shmtx->sem_id = semid; 

    return shmtx;
}

int
lock(confd_shmtx_t* shmtx)
{
    int    i;
    struct sembuf  sb[MAX_SEMAPHORE];

    for(i = 0; i < MAX_SEMAPHORE; ++i) {
        sb[i].sem_num = i ;
        sb[i].sem_op = -1 ;
        sb[i].sem_flg = 0 ;
    }

    return semop(shmtx->sem_id , sb , MAX_SEMAPHORE);
}

int
unlock(confd_shmtx_t* shmtx)
{
    int i;
    struct sembuf  sb[MAX_SEMAPHORE];

    for(i = 0; i < MAX_SEMAPHORE; ++i) {
        sb[i].sem_num = i ;
        sb[i].sem_op = +1 ;
        sb[i].sem_flg = 0 ;
    }

    return semop(shmtx->sem_id , sb , MAX_SEMAPHORE);
}


bool
destory_lock(confd_shmtx_t* shmtx)
{
    int sem_id;

    sem_id = shmtx->sem_id;
    free(shmtx);
    if (semctl(sem_id, 0, IPC_RMID, 0) == -1) {
        printf("semctl(IPC_RMID) failed error: %s\n", strerror(errno));
        return false;
    }
    return true;
}

#endif
