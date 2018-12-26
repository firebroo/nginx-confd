#ifndef _CONFD_SHMTX_H
#define _CONFD_SHMTX_H

#define MAX_SEMAPHORE 1

typedef struct confd_shmtx {
#if (USE_SYSV_SEM)
    int            sem_id; 
#else
    int            fd;
    unsigned char *name;
#endif
} confd_shmtx_t;

confd_shmtx_t* init_lock();
int lock(confd_shmtx_t* shmtx);
int unlock(confd_shmtx_t* shmtx);
bool destory_lock(confd_shmtx_t* shmtx);

#endif
