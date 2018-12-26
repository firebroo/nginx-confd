#include "../src/confd_shmtx.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>


int
main(void)
{
    confd_shmtx_t* shmtx = init_lock();
    if(!shmtx) {
        printf("init lock failed.");
        exit(-1);
    }
    pid_t pid = fork();
    if (pid < 0) {
        printf("fork failed.");
        exit(-1);
    }
    if (pid > 0) { //father
        printf("father process try get lock.\n");
        lock(shmtx);
        printf("father process get lock, and will sleep 5\n");
        sleep(5);
        unlock(shmtx);
        printf("father process unlock.\n");
    } else { //children
        sleep(1);
        printf("children process try get lock.\n");
        lock(shmtx);
        printf("children process get lock.\n");
        unlock(shmtx);
        printf("children unlock and exit.\n");
        exit(0);
    }
    int status;
    while ((pid = waitpid(-1, &status, 0)) != 0) {    //等待所有子进程退出
        if (errno == ECHILD) {
            printf("wait all children process exit finish.\n");
            break;
        }
    }
    printf("lock test successful.\n");
    destory_lock(shmtx);

    return 0;
}
