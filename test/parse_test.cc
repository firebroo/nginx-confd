#include "../src/nginx_conf_parse.h"
#include <iostream>

int
main(void)
{
    nginxConfParse p;
    unordered_map<std::string, vector<std::string>> confs = p.parse("/home/work/nginx/conf/nginx.conf");
    for(auto& item: confs) {
        for(auto& v: item.second) {
            std::cout << item.first  << "=>" << v << std::endl;
        }
    }
    if (confs.size() >= 1) {
        std::cout << "nginx conf parse ok." << std::endl;;
    } else {
        throw;
    };
}
