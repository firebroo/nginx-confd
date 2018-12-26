#ifndef _CONFD_SHM_H
#define _CONFD_SHM_H

#include "config.h"

typedef struct confd_shm {
    char* addr;
    size_t size;
} confd_shm_t;

confd_shm_t* init_shm(size_t shm_size);
bool destory_shm(confd_shm_t *shm);
//bool confd_shm_alloc(confd_shm_t *shm);
//bool confd_shm_free(confd_shm_t *shm);

#endif
