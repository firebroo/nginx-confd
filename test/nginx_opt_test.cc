#include "../src/nginx_opt.h"
#include <assert.h>
#include <unistd.h>
#include <string>

int
main(void)
{ 
    std::string key("abc.firebroo.com");
    std::vector<std::string> value;
    value.push_back("1.1.1.1:80");
    value.push_back("1.1.1.2:80");
    const char* nginx_bin_path = "/usr/sbin/nginx";
    const char* nginx_conf_path = "/etc/nginx/nginx.conf";
    std::pair<bool, std::string> ret;

    ret = nginx_opt::sync_to_disk("80", key, value, "standard", "/etc/nginx/vhost/", "/usr/sbin/nginx", "/etc/nginx/nginx.conf");
    if (ret.first) {
        std::cout << "nginx test ok, sync to disk successful" << std::endl;
    } else {
        std::cout << "nginx test error: " << ret.second << std::endl;
    }
    ret = nginx_opt::nginx_conf_reload(nginx_bin_path, nginx_conf_path);
    if (ret.first) {
        std::cout << "nginx test ok, reload successful." << std::endl;
    } else {
        std::cout << ret.second << std::endl;
    }
    ret = nginx_opt::nginx_conf_graceful_reload(nginx_bin_path, nginx_conf_path);
    if (ret.first) {
        std::cout << "nginx test ok, graceful successful." << std::endl;
    } else {
        std::cout << ret.second << std::endl;
    }
    ret = nginx_opt::nginx_conf_test(nginx_bin_path, nginx_conf_path);
    if (ret.first) {
        std::cout << "nginx test ok, test syntax successful." << std::endl;
    } else {
        std::cout << ret.second << std::endl;
    } 
    ret = nginx_opt::nginx_worker_used_memsum();
    if (ret.first) {
        std::cout << "nginx test ok, nginx_worker process usedmem: " << ret.second << std::endl;
    } else {
        std::cout << ret.second << std::endl;
    }
    ret = nginx_opt::nginx_shutting_worker_count();
    if (ret.first) {
        std::cout << "nginx test ok, shutting worker:  " << std::stoll(ret.second) << std::endl;
    } else {
        std::cout << ret.second << std::endl;
    }
    ret = nginx_opt::nginx_process_used_memsum();
    if (ret.first) {
        std::cout << "nginx test ok, nginx process usedmem:  " << ret.second << std::endl;
    } else {
        std::cout << ret.second << std::endl;
    }
}
