#include "nginx_opt.h"
#include "util.h"
#include "log.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <stdio.h>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
 
namespace logging = boost::log;
using namespace logging;

const char* nginx_opt::upstream_struct_start = "upstream %s {\n";
const char* nginx_opt::upstream_struct_server = "    server %s;\n";
const char* nginx_opt::upstream_struct_end = "}\n";
const char* nginx_opt::nginx_test_ok = "nginx: the configuration file %s syntax is ok\n"
                                       "nginx: configuration file %s test is successful\n";
const char* nginx_opt::nginx_http_health_check = "    check interval=10000 rise=2 fall=5 timeout=2000 type=http;\n"
                                                 "    check_http_send \"HEAD / HTTP/1.0\\r\\n\\r\\n\";\n"
                                                 "    check_http_expect_alive http_2xx http_3xx;\n";
const char* nginx_opt::nginx_tcp_health_check = "    check interval=10000 rise=2 fall=5 timeout=2000 type=tcp;\n";
health_check_map_t health_check_map[] = {
    {TCP,  nginx_opt::nginx_tcp_health_check},
    {HTTP, nginx_opt::nginx_http_health_check},
    {NONE, ""}
};


std::pair<bool, std::string> 
nginx_opt::nginx_conf_test(const char* nginx_bin_path, const char* nginx_conf_path)
{
    char  cmd[1024] = {0};
    char  test_ok[1024] = {0};

    sprintf(test_ok, nginx_opt::nginx_test_ok, nginx_conf_path, nginx_conf_path);
    sprintf(cmd, "%s -c %s -t 2>&1", nginx_bin_path, nginx_conf_path);

    std::pair<bool, std::string> ret = exec_cmd(cmd);
    if (!ret.first) {
        return ret;
    }
    std::string& buffer = ret.second;
    if (!strcmp(test_ok, buffer.c_str())) {
        return std::pair<bool, std::string>(true, "");
    }
    return std::pair<bool, std::string>(false, buffer);
}

std::pair<bool, std::string> 
nginx_opt::nginx_conf_reload(const char* nginx_bin_path, const char* nginx_conf_path)
{
    char  cmd[1024] = {0};

    sprintf(cmd, "%s -c %s -s reload 2>&1", nginx_bin_path, nginx_conf_path);
    std::pair<bool, std::string> ret = exec_cmd(cmd);
    if (!ret.first) {
        return ret;
    }
    std::string& buffer = ret.second;
    if (buffer.empty()) {
        return std::pair<bool, std::string>(true, "");
    }
    return std::pair<bool, std::string>(false, buffer);
}

std::pair<bool, std::string> 
nginx_opt::nginx_conf_graceful_reload(const char* nginx_bin_path, const char* nginx_conf_path)
{
    size_t used_memsum = 0;

    std::pair<bool, std::string> memsum = nginx_opt::nginx_worker_used_memsum();
    if (!memsum.first) {
        return memsum;
    } else {
        used_memsum += parse_bytes_number(memsum.second);
    }
    long MemAvailable = get_MemAvailable();
    if (MemAvailable == -1L) {
        return std::pair<bool, std::string>(false, "get key='MemAvailable' from /proc/meminfo failed.");
    }
    if (MemAvailable >= 0 && (size_t)MemAvailable < used_memsum) {
        return std::pair<bool, std::string>(false, "not enough memory.");
    }
    return nginx_opt::nginx_conf_reload(nginx_bin_path, nginx_conf_path);
}

std::string
nginx_opt::gen_upstream(std::string listen_port, std::string key, std::vector<std::string> value, HEALTH_CHECK type)
{
    char upstream[10240];
    char line_server[1024];
    std::string upstream_name = key+"_"+listen_port;

    std::string servers, upstream_format;
    for (auto& v: value) {
        sprintf(line_server, nginx_opt::upstream_struct_server, v.c_str());
        servers += std::string(line_server);
    }
    health_check_map_t *health_check_item;
    std::string health_check_content("");
    for (health_check_item = health_check_map; health_check_item->type != NONE; health_check_item++) {
        if (type == health_check_item->type) {
            health_check_content = health_check_item->content;
            break;
        } 
    } 
    upstream_format += std::string(nginx_opt::upstream_struct_start) + \
            health_check_content + std::string(servers) + std::string(nginx_opt::upstream_struct_end);
    sprintf(upstream, upstream_format.c_str(), upstream_name.c_str());
    return std::string(upstream);
}

std::string
nginx_opt::gen_server(std::string listen_port, std::string server_name, std::vector<std::string> value, std::string tmp_style)
{
    std::ifstream ifs; 
    const char *conf_extension = ".conf";

    std::string filename = "nginx_" + tmp_style + conf_extension;
    boost::filesystem::path file_path= boost::filesystem::path(filename); 
    if (!boost::filesystem::exists(file_path) || \
        !boost::filesystem::is_regular_file(file_path) || \
        boost::filesystem::extension(file_path) != conf_extension) {
        return "";
    }
    ifs.open(filename);
    if (!ifs) {
        return "";
    }
    std::ostringstream os;
    os << ifs.rdbuf();


    std::string nginx_conf_template = std::string(os.str());
    return nginx_opt::template_replace(listen_port, server_name, nginx_conf_template);
}

std::string 
nginx_opt::template_replace(const std::string& listen_port, const std::string& server_name, std::string nginx_conf_template)
{
    char buf[10240];

    std::string upstream_name = server_name + "_" + listen_port;

    std::unordered_map<std::string, std::string> replace_value{
        {"%{listen_port}",   listen_port},
        {"%{server_name}",   server_name},
        {"%{upstream_name}", upstream_name}
    };
    for (auto& item: replace_value) {
        while (true) {
            std::string copyed = boost::replace_first_copy(nginx_conf_template, item.first, "%s"); 
            if (copyed == nginx_conf_template) {
                break;
            }
            memset(buf, '\0', 10240);
            nginx_conf_template = copyed;
            sprintf(buf, nginx_conf_template.c_str(), item.second.c_str());
            nginx_conf_template = std::string(buf);
        }
    }

    return nginx_conf_template;
}

std::pair<bool, std::string>
nginx_opt::delete_conf(std::string& key, std::string& nginx_conf_writen_path)
{
    const char *conf_extension = ".conf";
    std::string filepath = nginx_conf_writen_path + key + conf_extension;
    if (!boost::filesystem::exists(filepath)) {
        return std::pair<bool, std::string>(false, "unlink filepath("+filepath+") failed, filepath is not exist.");
    }
    unlink(filepath.c_str());
    return std::pair<bool, std::string>(true, "unlink filepath("+filepath+") successful.");
}

std::pair<bool, std::string>
nginx_opt::sync_to_disk(std::string port, std::string key, std::vector<std::string> value, 
    std::string tmp_style, std::string nginx_conf_writen_path, std::string nginx_bin_path, 
    std::string nginx_conf_path, HEALTH_CHECK type)
{
    std::string upstream = nginx_opt::gen_upstream(port, key, value, type);
    if (upstream.empty()) {
        return std::pair<bool, std::string>(false, "");
    }
    std::string server = nginx_opt::gen_server(port, key, value, tmp_style);
    if (server.empty()) {
        return std::pair<bool, std::string>(false, "nginx conf template error.");
    }
    std::ofstream ofs;
    char filepath[10240];
    sprintf(filepath, "%s%s_%s.conf", nginx_conf_writen_path.c_str(), key.c_str(), port.c_str());
    ofs.open(filepath);
    if (!ofs) {
        std::stringstream ss;
        ss << "Open file(" << filepath << ") failed.";
        BOOST_LOG_TRIVIAL(error) << ss.str();
        return std::pair<bool, std::string>(false, ss.str());
    }
    ofs << upstream << server;
    ofs.close();
    auto ret = nginx_opt::nginx_conf_test(nginx_bin_path.c_str(), nginx_conf_path.c_str());
    if(!ret.first) {
        BOOST_LOG_TRIVIAL(info) << ret.second << upstream << server;
        unlink(filepath);
        return ret;
    }
    ret = nginx_opt::nginx_conf_graceful_reload(nginx_bin_path.c_str(), nginx_conf_path.c_str());
    if(!ret.first) {
        unlink(filepath);
        return ret;
    }
    return std::pair<bool, std::string>(true, "add successful.");
}

std::pair<bool, std::string>
nginx_opt::nginx_worker_used_memsum()
{
    const char* cmd = "ps aux | grep 'nginx: worker process' | grep -v 'shutdown' | "
                      "grep -v 'grep'| awk 'BEGIN{total=0}{total+=$6}END{printf 'total'}'";
    std::pair<bool, std::string> ret = exec_cmd(cmd);
    if (!ret.first) {
        return ret;
    }

    ret.second += "kb"; //patch unit kb
    return ret;
}

std::pair<bool, std::string>
nginx_opt::nginx_shutting_worker_count(void)
{
    const char* cmd = "ps aux | grep nginx | grep shutting | grep -v 'grep' | wc -l";
    return exec_cmd(cmd);
}

std::pair<bool, std::string>
nginx_opt::nginx_process_used_memsum(void)
{
    const char* cmd = "ps aux | grep 'nginx:' | grep -v 'grep'| awk 'BEGIN{total=0}{total+=$6}END{printf 'total'}'";
    std::pair<bool, std::string> ret = exec_cmd(cmd);
    if (!ret.first) {
        return ret;
    }
    ret.second = std::to_string(std::stoll(ret.second) / 1024);
    ret.second += "Mb"; //patch unit kb
    return ret;
}

HEALTH_CHECK int_to_health_check_type(int value)
{
    HEALTH_CHECK type;

    switch (value) {

    case 0:
        type = NONE;
        break;
    case 1:
        type = TCP;
        break;
    case 2:
        type = HTTP;
        break;
    default:
        type = NONE;
        break;
    }
    
    return type;
}

bool
nginx_opt::backup_single_conf(std::string& file_path)
{
    int ret;

    if (!boost::filesystem::exists(file_path) || !boost::filesystem::is_regular_file(file_path)) {
        return true;
    }

    ret = rename(file_path.c_str(), (file_path+".bak").c_str());
    if (ret == -1) {
        return false;
    }

    return true;
}

bool
nginx_opt::rollback_single_conf(std::string& file_path)
{
    int ret;

    std::string back_conf(file_path+".bak");
    if (!boost::filesystem::exists(back_conf) || !boost::filesystem::is_regular_file(back_conf)) {
        return true;
    }
    ret = rename(back_conf.c_str(), file_path.c_str());
    if (ret == -1) {
        return false;
    }
    return true;
}
