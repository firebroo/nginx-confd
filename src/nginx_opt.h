#ifndef _NGINX_OPT_H
#define _NGINX_OPT_H

#include <string>
#include <vector>
#include <iostream>

class nginx_opt { 
    public:
      static const char* upstream_struct_start;
      static const char* upstream_struct_server;
      static const char* upstream_struct_end;
      static const char* nginx_test_ok;
    public:
      static std::string gen_upstream(std::string listen_port, std::string key, std::vector<std::string> value);
      static std::string gen_server(std::string listen_port, std::string key, std::vector<std::string> value, std::string tmp_style);
      static std::pair<bool, std::string> nginx_conf_test(const char* nginx_bin_path, const char* nginx_conf_path);
      static std::pair<bool, std::string> nginx_conf_reload(const char* nginx_bin_path, const char* nginx_conf_path);
      static std::pair<bool, std::string> sync_to_disk(std::string port, std::string key, std::vector<std::string> value, 
            std::string tmp_style, std::string nginx_conf_writen_path, std::string nginx_bin_path, std::string nginx_conf_path);
};

#endif
