#include "../src/nginx_conf_parse.h"
#include <iostream>

int
main(void)
{
    nginxConfParse p;
    unordered_map<std::string, vector<std::string>> confs = p.parse("/etc/nginx/nginx.conf");
    for (auto& iter: confs) {
        std::cout << iter.first << std::endl;        
    }
}
