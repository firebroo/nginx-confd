#ifndef _PROCESS_MANAGE_H
#define _PROCESS_MANAGE_H

#include "config.h"

#define MASTERNAME "confd: master process"
#define WORKERNAME "confd: worker process"

typedef struct confd_signal {
    int         signo;
    const char *opt_name;
    void (*handler)(int);
} confd_signal_t;

void graceful_shutdown_confd(int signum);
void notify_master_process(const char *pid_path, const char *cmd);
bool init_master_process(char* process_name_ptr, unordered_map<string,string> config);
void init_worker_process(char* process_name_ptr, unordered_map<string,string> config);

#endif
