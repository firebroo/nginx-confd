#include "../src/nginx_opt.h"
#include <assert.h>

int
main(void)
{ 
    std::string key("abc.firebroo.com");
    std::vector<std::string> value;
    value.push_back("1.1.1.1:80");
    value.push_back("1.1.1.2:80");

    auto ret = nginx_opt::sync_to_disk("80", key, value, "standard", "/etc/nginx/vhost/", "/usr/sbin/nginx", "/etc/nginx/nginx.conf");
    std::cout << ret.second << std::endl;
}
