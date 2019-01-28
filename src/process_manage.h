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

typedef struct c_process {
    std::string  name;
    pid_t        pid;
    bool         restart;
} process_t;

void notify_master_process(const char *pid_path, const char *cmd);
bool init_master_process(char* process_name_ptr, unordered_map<string,string> config);
void init_worker_process(char* process_name_ptr, unordered_map<string,string> config, std::string name);
pid_t spawn_worker_process(std::string name);

#endif
