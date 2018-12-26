#include "../src/nginx_conf_parse.h"
#include <iostream>

int
main(void)
{
    nginxConfParse p;
    unordered_map<std::string, vector<std::string>> confs = p.parse("/etc/nginx/nginx.conf");
    if (confs.size() >= 25) {
        std::cout << "nginx conf parse ok." << std::endl;;
    } else {
        throw;
    };
}
