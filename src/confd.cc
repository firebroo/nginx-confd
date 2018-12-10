#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <fstream>
#include <unordered_map>
#include <vector>
#include "pv.h"
#include "nginx_conf_parse.h"
#include "mem_dict.h"
#include "log.h"
#include "util.h"
#include "httpd.h"
#include "config.h"

#define MASTERNAME "confd: master process"
#define WORKERNAME "confd: worker process"

namespace logging = boost::log;
using namespace logging;

char *data;
int   sem_id;
vector<pid_t> children_process_group;
unordered_map<string,string> confd_config;

std::pair<bool, std::string>
load_nginx_conf()
{
    nginxConfParse nginx_conf_parse;
    auto ret = nginx_opt::nginx_conf_test((confd_config["nginx_bin_path"].c_str()), confd_config["nginx_conf_path"].c_str());
    if (!ret.first) {
        return ret;
    }
    confd_dict* confd_p = new confd_dict(nginx_conf_parse.parse(confd_config["nginx_conf_path"].c_str()));
    confd_p->sync_to_shm();        
    delete confd_p;

    return std::pair<bool, std::string>(true, "");
}

void
reload_nginx_conf(int signum)
{
    signal(SIGUSR1, reload_nginx_conf);
    std::pair<bool, std::string> ret = load_nginx_conf();
    if (!ret.first) {
        BOOST_LOG_TRIVIAL(warning) << "confd: reload nginx failed error: " << ret.second;
    } else {
        BOOST_LOG_TRIVIAL(info) << "confd: reload nginx successful.";
    }
}


void
graceful_shutdown_confd(int signum)
{
    for(auto& pid: children_process_group) { //exit all children process
        if (kill(pid, SIGKILL)) {
            printf("confd: kill failed: pid(%d)\n", pid);
        }
    } 
    rm_pv(sem_id);
    exit(0); // exit master process
}

void
writen_pidfile(const char* pid_path)
{
    ofstream ofs; 
    pid_t pid = getpid();

    ofs.open(pid_path);
    if (!ofs) {
        printf("Open pid_file(%s) error: %s\n", pid_path, strerror(errno)); 
        exit(-1);
    }
    ofs << pid << "\n";
    ofs.close();
}

pid_t
get_master_prcess_pid(const char* pid_path)
{
    pid_t pid;
    ifstream ifs;

    ifs.open(pid_path);
    if (!ifs) {
        return -1;
    }
    ifs >> pid;
    return pid;
}


void
alloc_share_mem(string& ipc_name, size_t size)
{
    int fd;

    fd = open(ipc_name.c_str(), O_RDWR | O_CREAT | O_TRUNC);
    if (fd < 0) {
        perror("open");
        exit(-1);
    }
    ftruncate(fd, size);
    
    data = (char *)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        exit(-1);
    }
    close(fd);
}

void
usage()
{
    printf("Usage: confd [-h] -c config\n"); 
    exit(-1);
}

void
notify_master_process(const char *pid_path, const char *cmd)
{

    pid_t pid = get_master_prcess_pid(pid_path);
    if (pid == -1) {
        printf("confd: open pid_file(%s) error: %s\n", pid_path, strerror(errno));
        exit(-1);
    }

    if (!strcmp(cmd, "reload")) {
        if (kill(pid, SIGUSR1)) {
            printf("confd: reload failed error: no such pid(%d)\n", pid);
            exit(-1);
        }
    } else if (!strcmp(cmd, "stop")) {
        if (kill(pid, SIGUSR2)) {
            printf("confd: reload failed error: no such pid(%d)\n", pid);
            exit(-1);
        }
    } else {
        printf("confd: unknown command option(%s)", cmd);
        exit(-1);
    }
}

bool
parse_config(unordered_map<string,string>& confd_config, const char* config_file)
{
    Json::Value config;

    if (!config_file) {
        printf("Configuration files must be specified\n");
        return false;
    }

    ifstream fin(config_file);
    if (!fin) {
        printf("confd: open config file(%s) failed: %s\n", config_file, strerror(errno));
        return false;
    }

    fin >> config;
    fin.close();

    if (!config["pid_path"].isNull()) {
        confd_config["pid_path"] = config["pid_path"].asString();
    }
    if (!config["addr"].isNull()) {
        confd_config["adrr"] = config["addr"].asString();
    }
    if (!config["port"].isNull()) {
        confd_config["port"] = config["port"].asString();
    }
    if (!config["nginx_conf_path"].isNull()) {
        confd_config["nginx_conf_path"] = config["nginx_conf_path"].asString();
    }
    if (!config["shm_size"].isNull()) {
        confd_config["shm_size"] = config["shm_size"].asString();
    }
    if (!config["ipc_name"].isNull()) {
        confd_config["ipc_name"] = config["ipc_name"].asString();
    }
    if (!config["nginx_conf_writen_path"].isNull()) {
        confd_config["nginx_conf_writen_path"] = config["nginx_conf_writen_path"].asString();
    }
    if (!config["nginx_bin_path"].isNull()) {
        confd_config["nginx_bin_path"] = config["nginx_bin_path"].asString();
    }

    return true;
}


int
main(int argc, char *argv[])
{
    int i;
    const char *cmd = NULL;
    const char *config_file = NULL;

    while ((i = getopt(argc, argv, "hs:c:")) != -1) {
        switch(i){
            case 'h':
                usage();
                return -1;
            case 's':
                cmd = optarg;
                break;
            case 'c':
                config_file = optarg;
                break;
            default:
                break;
        }
    }

    if (!parse_config(confd_config, config_file)) {
        exit(-1);
    };

    if (cmd != NULL) {
        notify_master_process(confd_config["pid_path"].c_str(), cmd);
        exit(0);
    } else {
        writen_pidfile(confd_config["pid_path"].c_str());
    }

    alloc_share_mem(confd_config["ipc_name"], parse_bytes_number(confd_config["shm_size"]));

    init_log(logging::trivial::debug, std::string("confd.log"));
    sem_id = init_pv();
    int pid = fork();
    if (pid < 0) {
        printf("fork error\n");
    }
    if (pid > 0) { //father
        children_process_group.push_back(pid);
        processname_rename(argv[0], MASTERNAME);

        std::pair<bool, std::string> ret = load_nginx_conf();
        if (!ret.first) {
            std::cout << "confd: load nginx failed error: " << ret.second;
            if (kill(pid, SIGKILL)) { //exit children
                printf("confd: kill failed: pid(%d)\n", pid);
            }
            exit(-1); //exit master
        }
        signal(SIGUSR1, reload_nginx_conf);
        signal(SIGUSR2, graceful_shutdown_confd);
        while (true) {
            sleep(-1);
        }
    } else {
        sleep(1); //wait for father load config
        processname_rename(argv[0], WORKERNAME);
        httpServer(confd_config["addr"], stoll(confd_config["port"])); 
    }
    return 0;    
}
