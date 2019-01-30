#include "log.h"
#include "util.h"
#include "config.h"
#include "confd_shm.h"
#include "confd_dict.h"
#include "process_manage.h"
#include "nginx_conf_parse.h"
#include "json.h"

namespace logging = boost::log;
using namespace logging;

std::vector<process_t> children_process_group;
std::unordered_map<std::string, std::string> confd_config;

void
usage()
{
    printf("Usage: confd [-h] -c config\n"); 
    exit(0);
}


bool
parse_config(unordered_map<string,string>& confd_config, const char* config_file)
{
    Json::Value config;

    if (!config_file) {
        printf("configuration files must be specified\n");
        return false;
    }

    ifstream fin(config_file);
    if (!fin) {
        printf("open config file(%s) failed: %s\n", config_file, strerror(errno));
        return false;
    }

    fin >> config;
    fin.close();

    if (!config["pid_path"].isNull()) {
        confd_config["pid_path"] = config["pid_path"].asString();
    }
    if (!config["addr"].isNull()) {
        confd_config["addr"] = config["addr"].asString();
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
    if (!config["nginx_conf_writen_path"].isNull()) {
        confd_config["nginx_conf_writen_path"] = config["nginx_conf_writen_path"].asString();
    }
    if (!config["nginx_bin_path"].isNull()) {
        confd_config["nginx_bin_path"] = config["nginx_bin_path"].asString();
    }
    if (!config["log_path"].isNull()) {
        confd_config["log_path"] = config["log_path"].asString();
    }
    if (!config["worker_process"].isNull()) {
        confd_config["worker_process"] = config["worker_process"].asString();
    }
    if (!config["verbosity"].isNull()) {
        confd_config["verbosity"] = config["verbosity"].asString();
    }
    if (!config["daemon"].isNull()) {
        confd_config["daemon"] = config["daemon"].asString();
    }

    return true;
}



bool
confd_daemon(void)
{
    int  fd;

    switch (fork()) {
    case -1:
        BOOST_LOG_TRIVIAL(error) << "fork failed.";
        return false;

    case 0:
        break;

    default:
        exit(0);
    }

    if (setsid() == -1) {
        BOOST_LOG_TRIVIAL(warning) << "setsid() failed.";
        return false;
    }

    umask(0);

    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
        BOOST_LOG_TRIVIAL(warning) << "open(\"/dev/null\") failed.";
        return false;
    }

    if (dup2(fd, STDIN_FILENO) == -1) {
        BOOST_LOG_TRIVIAL(warning) << "dup2(STDIN) failed.";
        return false;
    }

    if (dup2(fd, STDOUT_FILENO) == -1) {
        BOOST_LOG_TRIVIAL(warning) << "dup2(STDOUT) failed.";
        return false;
    }

    if (dup2(fd, STDERR_FILENO) == -1) {
        BOOST_LOG_TRIVIAL(warning) << "dup2(STDERR) failed.";
        return false;
    }

    if (fd > STDERR_FILENO) {
        if (close(fd) == -1) {
            BOOST_LOG_TRIVIAL(warning) << "close fd(" << fd << ") failed.";
            return false;
        }
    }

    return true;
}



int
main(int argc, char *argv[])
{
    int i;
    const char *cmd = NULL;
    const char *config_file = NULL;
    confd_arg = {
        .argc = argc,
        .argv = argv
    };

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

    //parse config
    if (!parse_config(confd_config, config_file)) {
        exit(-1);
    };


    //confd opt cmd
    if (cmd != NULL && *cmd != '\0') {
        notify_master_process(confd_config["pid_path"].c_str(), cmd);
    }

    //初始化log
    try {
        init_log(stoll(confd_config["verbosity"]), confd_config["log_path"]);
    } catch (std::exception& e) {
        printf("init log failed error: %s", e.what());
    }

    //daemonize?
    if (confd_config["daemon"] == "true") {
        if (confd_daemon()) {
            BOOST_LOG_TRIVIAL(debug) << "daemonize successufl.";
        } else {
            BOOST_LOG_TRIVIAL(warning) << "daemonize failed.";
        }
    }

    //初始化主进程
    if (!init_master_process(confd_config)) {
        exit(-1);
    }

    //fork worker process
    for (int i = 0; i < stoll(confd_config["worker_process"]); i++) {
        std::string name = std::string("worker") + std::to_string(i);
        pid_t pid = spawn_worker_process(name);
        if (pid == -1) {
            BOOST_LOG_TRIVIAL(warning) << "spawn worker process failed";
            continue;
        }
        process_t new_process = {
            .name = name,
            .pid = pid,
            .restart = 1 
        };
        children_process_group.push_back(new_process);
    }


    //master process will sleep util received signal
    while (true) {
        sleep(-1);
    }
    return 0;    
}
