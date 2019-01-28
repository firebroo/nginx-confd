#include "log.h"
#include "httpd.h"
#include "confd_shm.h"
#include "confd_shmtx.h"
#include "process_manage.h"
#include "confd_dict.h"
#include "nginx_conf_parse.h"

static void reload_nginx_conf(int signum);
static void waitpid_handler(int signum);
static void graceful_shutdown_confd(int signum);

static std::pair<bool, std::string> load_nginx_conf(const std::string& nginx_bin_path, 
        const std::string& nginx_conf_path);
static bool register_signals(confd_signal_t *signals);
static pid_t get_master_process_pid(const char* pid_path);
static bool writen_pidfile(const char* pid_path);

confd_shmtx_t* shmtx;
confd_shm_t* shm;
confd_shmtx_t* updatetx;
confd_shm_t* update;
extern std::vector<process_t> children_process_group;
extern std::unordered_map<std::string, std::string> confd_config;
extern char *process_name_ptr;

static confd_signal_t signals[] = {
    {SIGHUP,  "reload",  reload_nginx_conf},
    {SIGTERM, "stop",    graceful_shutdown_confd},
    {SIGCHLD, "sigchld", waitpid_handler},
    {0,       NULL,      NULL}                       //loop end flag
};

bool
register_signals(confd_signal_t *signals)
{
    confd_signal_t  *sig;
    struct sigaction sa;
    sa.sa_flags = 0;

    for(sig = signals; sig->signo != 0; sig++) {
        sa.sa_handler = sig->handler;
        if (sigaction(sig->signo, &sa, NULL) == -1) {
            BOOST_LOG_TRIVIAL(error) << "regesiter signal(" << sig->signo << ")failed.";
            return false;
        } else {
            BOOST_LOG_TRIVIAL(debug) << "regesiter signal(" << sig->signo << ")successful.";
        }

    }

    return true;
}

bool
reset_signals(confd_signal_t *signals)
{
    confd_signal_t  *sig;
    struct sigaction sa;
    sa.sa_flags = 0;

    for(sig = signals; sig->signo != 0; sig++) {
        sa.sa_handler = SIG_DFL;
        if (sigaction(sig->signo, &sa, NULL) == -1) {
            BOOST_LOG_TRIVIAL(error) << "reset signal(" << sig->signo << ")failed.";
            return false;
        } else {
            BOOST_LOG_TRIVIAL(debug) << "reset signal(" << sig->signo << ")successful.";
        }

    }

    return true;
}

void
waitpid_handler(int signum)
{
    int        status;
    pid_t      pid;
    u_int      one;

    one = 0;
    for ( ;; ) {
        pid = waitpid(-1, &status, WNOHANG);
        if (pid == 0) {
            return;
        }
        if (pid == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == ECHILD && one) {
                return;
            }
            if (errno == ECHILD) {
                BOOST_LOG_TRIVIAL(info) << "waitpid() failed";
                return;
            }
            BOOST_LOG_TRIVIAL(warning) << "waitpid() failed";
            return;
        }

        one = 1;
        if (WTERMSIG(status)) {
            BOOST_LOG_TRIVIAL(warning) << "pid(" << pid 
                                       << ") exited on signal=" << WTERMSIG(status)
                                       << " status=" << WEXITSTATUS(status);
        } else {
            BOOST_LOG_TRIVIAL(info) << "pid(" << pid 
                                    << ") exited with code=" << WEXITSTATUS(status);
        }

        vector<process_t>::iterator it;
        for (it = children_process_group.begin(); it != children_process_group.end(); it++) {
            if ((*it).pid == pid) {
                break;
            }
        }

        if (it == children_process_group.end()) { /*没有找到该pid*/
            continue;
        }

        process_t old_process = (*it);

        //从子进程组里面删除此子进程
        children_process_group.erase(it);


        BOOST_LOG_TRIVIAL(info) << "process(" << old_process.name << ") exit";

        if (WEXITSTATUS(status) != 0) {           /*致命错误，跳过*/
            BOOST_LOG_TRIVIAL(warning) << "pid(" << pid 
                                       << ") exited with fatal code=" << WEXITSTATUS(status);
            continue;
        }

        if (old_process.restart == 0) { /*无需重启*/
            continue;
        }

        //正常退出，尝试重启
        pid_t new_pid;
        if ((new_pid = spawn_worker_process(old_process.name)) == -1) {
            BOOST_LOG_TRIVIAL(warning) << "spawn worker process failed";
            continue;
        }
        process_t new_process = old_process;
        new_process.pid = new_pid;
        children_process_group.push_back(new_process);
        BOOST_LOG_TRIVIAL(info) << "process(" << new_process.name << ") spawn successful";
    }
}

pid_t
spawn_worker_process(const std::string name)
{
    pid_t   pid;

    pid = fork();
    switch (pid) {

    case -1:
        BOOST_LOG_TRIVIAL(warning) << "fork() failed";
        return -1;
    case 0:
        init_worker_process(process_name_ptr, confd_config, name);
        break;
    default:
        break;
    }

    return pid;
}

void
graceful_shutdown_confd(int signum)
{
    BOOST_LOG_TRIVIAL(debug) << "received signal(" << signum << "), shutdown all process.";
    for (auto& children: children_process_group) { //exit all children process
        children.restart = 0;                      //禁止重启子进程
    }
    std::vector<process_t> copyed(children_process_group); 
    for (auto& children: copyed) { //exit all children process
        BOOST_LOG_TRIVIAL(debug) << "send sigkill to pid(" << children.pid << ")";
        if (kill(children.pid, SIGKILL)) {
            BOOST_LOG_TRIVIAL(warning) << "send sigkill to pid(" << children.pid << ") failed";
        }
    } 

    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, 0)) != 0) {    //等待所有子进程退出
        if (errno == ECHILD) {
            BOOST_LOG_TRIVIAL(debug) << "wait all children process exit finish.";
            break;
        }
    }

    if (destory_lock(shmtx)) {     //delete lock
        BOOST_LOG_TRIVIAL(info) << "shmtx lock destory successful.";
    }

    if (destory_shm(shm)) {
        BOOST_LOG_TRIVIAL(info) << "shm destory successful.";
    }

    if (destory_lock(updatetx)) {     //delete lock
        BOOST_LOG_TRIVIAL(info) << "updatetx lock destory successful.";
    }

    if (destory_shm(update)) {
        BOOST_LOG_TRIVIAL(info) << "update shm destory successful.";
    }

    BOOST_LOG_TRIVIAL(debug) << "shutdown all process finish.";
    exit(0);                                //退出主进程
}

bool
writen_pidfile(const char* pid_path)
{
    ofstream ofs; 
    pid_t pid = getpid();

    ofs.open(pid_path);
    if (!ofs) {
        BOOST_LOG_TRIVIAL(error) << "open pid_path(" << pid_path << ") failed.";
        return false;
    }
    ofs << pid << "\n";
    ofs.close();
    return true;
}

pid_t
get_master_process_pid(const char* pid_path)
{
    pid_t pid;
    ifstream ifs;

    ifs.open(pid_path);
    if (!ifs) {
        return -1;
    }
    ifs >> pid;
    ifs.close();
    return pid;
}

void
notify_master_process(const char *pid_path, const char *cmd)
{

    pid_t pid = get_master_process_pid(pid_path);
    if (pid == -1) {
        printf("confd: open pid_file(%s) error: %s\n", pid_path, strerror(errno));
        exit(-1);
    }

    confd_signal_t* signal;
    for (signal = signals; signal->signo != 0; signal++) {
        if (!strcmp(cmd, signal->opt_name)) {
            if (kill(pid, signal->signo) == -1) {
                printf("%s failed error: pid=%d, errno=%s\n", signal->opt_name, pid, strerror(errno));
            }
            break;
        }
    }
    if (signal->signo == 0) {
        printf("opt failed error: unknown confd optno(%s)\n", cmd);
        exit(-1);
    }
    exit(0);
}

std::pair<bool, std::string>
load_nginx_conf(const std::string& nginx_bin_path, const std::string& nginx_conf_path)
{
    std::string status;
    nginxConfParse nginx_conf_parse;

    std::pair<bool, std::string> ret = nginx_opt::nginx_conf_test(nginx_bin_path.c_str(), nginx_conf_path.c_str());
    if (!ret.first) {
        return ret;
    }
    std::unordered_map<std::string, vector<std::string>> dict = nginx_conf_parse.parse(nginx_conf_path.c_str()); 
    confd_dict* confd_p = new confd_dict(dict);
    std::string new_data = confd_p->json_stringify();
    lock(shmtx);
    if (strcmp(shm->addr, new_data.c_str())) {
        strcpy(shm->addr, new_data.c_str());
        status = "update";
    } else {
        status = "no update";
    }
    unlock(shmtx);
    delete confd_p;

    return std::pair<bool, std::string>(true, status);
}

void
reload_nginx_conf(int signum)
{
    std::pair<bool, std::string> ret = load_nginx_conf(confd_config["nginx_bin_path"], confd_config["nginx_conf_path"]);
    if (!ret.first) {
        BOOST_LOG_TRIVIAL(warning) << "reload nginx failed error: " << ret.second;
    } else {
        if (ret.second == "update") {
            confd_dict dict;
            dict.update_status(true);
        }
        BOOST_LOG_TRIVIAL(info) << "reparse && reload nginx_conf successful, status: " << ret.second;
    }
}

bool
init_master_process(char* process_name_ptr, unordered_map<string,string> config)
{

    process_rename(process_name_ptr, MASTERNAME);

    if (!writen_pidfile(config["pid_path"].c_str())) {
        BOOST_LOG_TRIVIAL(warning) << "writed pidfile(" << config["pid_path"] << ") failed.";
        return false;
    }

    update = init_shm(sizeof(bool) * 4096);
    if (!update) {
        BOOST_LOG_TRIVIAL(warning) << "init update shm failed.";
    }

    updatetx = init_lock();
    if (!updatetx) {
        BOOST_LOG_TRIVIAL(error) << "int updatetx lock failed.";
        return false;
    }

    shm = init_shm(parse_bytes_number(config["shm_size"]));
    if (!shm) {
        BOOST_LOG_TRIVIAL(warning) << "init shm failed.";
    }

    shmtx = init_lock();
    if (!shmtx) {
        BOOST_LOG_TRIVIAL(error) << "int shmtx lock failed.";
        return false;
    }

    std::pair<bool, std::string> ret = load_nginx_conf(config["nginx_bin_path"], config["nginx_conf_path"]);
    if (!ret.first) {
        BOOST_LOG_TRIVIAL(error) << "load nginx conf error: " << ret.second;
        return false;
    }
    
    if (!register_signals(signals)) {
        BOOST_LOG_TRIVIAL(error) << "register signal failed.";
        return false;
    }

    return true;
}

void
init_worker_process(char* process_name_ptr, unordered_map<string,string> config, std::string name)
{
    process_rename(process_name_ptr, WORKERNAME);

    BOOST_LOG_TRIVIAL(error) << "start#########process(" << name << ") reset signals############start";
    if (!reset_signals(signals)) {
        BOOST_LOG_TRIVIAL(error) << "reset signal failed.";
    }
    BOOST_LOG_TRIVIAL(error) << "end##########process(" << name << ") reset signals############end";

    httpServer(config["addr"], stoll(config["port"])); 
    exit(0);
}
