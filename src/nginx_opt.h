#ifndef _NGINX_OPT_H
#define _NGINX_OPT_H

#include <string>
#include <vector>
#include <iostream>

typedef enum health_check {
    NONE,
    TCP,
    HTTP
} HEALTH_CHECK;

typedef struct {
    HEALTH_CHECK type;
    const char*  content; 
} health_check_map_t;

HEALTH_CHECK int_to_health_check_type(int value);

class nginx_opt { 
    public:
      static const char* upstream_struct_start;
      static const char* upstream_struct_server;
      static const char* upstream_struct_end;
      static const char* nginx_test_ok;
      static const char* nginx_http_health_check;
      static const char* nginx_tcp_health_check;
    private:
      static std::string template_replace(const std::string& listen_port, const std::string& server_name, \
                                            std::string nginx_conf_template);
    public:
      static std::string gen_upstream(std::string listen_port, std::string key, std::vector<std::string> value, HEALTH_CHECK type);
      static std::string gen_server(std::string listen_port, std::string key, \
                                        std::vector<std::string> value, std::string tmp_style);
      static std::pair<bool, std::string> nginx_conf_test(const char* nginx_bin_path, const char* nginx_conf_path);
      static std::pair<bool, std::string> nginx_conf_reload(const char* nginx_bin_path, const char* nginx_conf_path);
      static std::pair<bool, std::string> nginx_conf_graceful_reload(const char* nginx_bin_path, const char* nginx_conf_path);
      static std::pair<bool, std::string> sync_to_disk(std::string port, std::string key, std::vector<std::string> value, \
            std::string tmp_style, std::string nginx_conf_writen_path, std::string nginx_bin_path, \
            std::string nginx_conf_path, HEALTH_CHECK type);
      static std::pair<bool, std::string> nginx_worker_used_memsum(void);
      static std::pair<bool, std::string> nginx_shutting_worker_count(void);
      static std::pair<bool, std::string> nginx_process_used_memsum(void);
      static std::pair<bool, std::string> delete_conf(std::string& key, std::string& nginx_conf_writen_path);
      static bool backup_single_conf(std::string& file_path);
      static bool rollback_single_conf(std::string& file_path);
};

#endif
