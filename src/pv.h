#ifndef _PV_H
#define _PV_H

#define MAX_SEMAPHORE 10

int init_pv();
int P(int semid);
int V(int semid);
void rm_pv(int sem_id);

#endif

