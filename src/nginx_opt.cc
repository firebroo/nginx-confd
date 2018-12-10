#include "nginx_opt.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <stdio.h>
#include <vector>
#include <boost/filesystem.hpp>
 
 
const char* nginx_opt::upstream_struct_start = "upstream %s {\n";
const char* nginx_opt::upstream_struct_server = "    server %s;\n";
const char* nginx_opt::upstream_struct_end = "}\n";
const char* nginx_opt::nginx_test_ok = "nginx: the configuration file %s syntax is ok\n"
                                       "nginx: configuration file %s test is successful\n";

std::pair<bool, std::string> 
nginx_opt::nginx_conf_test(const char* nginx_bin_path, const char* nginx_conf_path)
{
    FILE *fp;
    char  cmd[1024] = {0};
    char  test_ok[1024] = {0};
    char  buffer[10240] = {0};

    sprintf(test_ok, nginx_opt::nginx_test_ok, nginx_conf_path, nginx_conf_path);

    sprintf(cmd, "%s -c %s -t 2>&1", nginx_bin_path, nginx_conf_path);
    fp = popen(cmd, "r");
    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    rewind(fp); 
    fread(buffer, sizeof(char), length, fp);
    fclose(fp);
    if (!strcmp(test_ok, buffer)) {
        return std::pair<bool, std::string>(true, "");
    }
    return std::pair<bool, std::string>(false, buffer);
}

std::pair<bool, std::string> 
nginx_opt::nginx_conf_reload(const char* nginx_bin_path, const char* nginx_conf_path)
{
    FILE *fp;
    char  cmd[1024] = {0};
    char  buffer[1024] = {0};

    sprintf(cmd, "%s -c %s -s reload 2>&1", nginx_bin_path, nginx_conf_path);
    fp = popen(cmd, "r");
    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    rewind(fp); 
    fread(buffer, sizeof(char), length, fp);
    fclose(fp);
    if (!strcmp("", buffer)) {
        return std::pair<bool, std::string>(true, "");
    }
    return std::pair<bool, std::string>(false, buffer);
}

std::string
nginx_opt::gen_upstream(std::string listen_port, std::string key, std::vector<std::string> value)
{
    char upstream[10240];
    char line_server[1024];
    std::string upstream_name = key+"_"+listen_port;

    std::string servers, upstream_format;
    for (auto& v: value) {
        sprintf(line_server, nginx_opt::upstream_struct_server, v.c_str());
        servers += std::string(line_server);
    }
    upstream_format += std::string(nginx_opt::upstream_struct_start) + \
            std::string(servers) + std::string(nginx_opt::upstream_struct_end);
    sprintf(upstream, upstream_format.c_str(), upstream_name.c_str());
    return std::string(upstream);
}

std::string
nginx_opt::gen_server(std::string listen_port, std::string key, std::vector<std::string> value, std::string tmp_style)
{
    char buf[10240];
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
    std::string upstream_name = key+"_"+listen_port;
    sprintf(buf, nginx_conf_template.c_str(), listen_port.c_str(), key.c_str(), upstream_name.c_str());
    return std::string(buf);
}

std::pair<bool, std::string>
nginx_opt::sync_to_disk(std::string port, std::string key, std::vector<std::string> value, 
    std::string tmp_style, std::string nginx_conf_writen_path, std::string nginx_bin_path, std::string nginx_conf_path)
{
    std::string upstream = nginx_opt::gen_upstream(port, key, value);
    if (upstream.empty()) {
        return std::pair<bool, std::string>(false, "");
    }
    std::string server = nginx_opt::gen_server(port, key, value, tmp_style);
    if (server.empty()) {
        return std::pair<bool, std::string>(false, "nginx conf template error.");
    }
    std::ofstream ofs;
    char filepath[10240];
    sprintf(filepath, "%s%s:%s.conf", nginx_conf_writen_path.c_str(), key.c_str(), port.c_str());
    //std::cout << filepath << std::endl;
    ofs.open(filepath);
    if (!ofs) {
        return std::pair<bool, std::string>(false, "");
    }
    ofs << upstream << server;
    ofs.close();
    //std::cout << upstream << server << std::endl;
    auto ret = nginx_opt::nginx_conf_test(nginx_bin_path.c_str(), nginx_conf_path.c_str());
    if(!ret.first) {
        unlink(filepath);
        return ret;
    }
    ret = nginx_opt::nginx_conf_reload(nginx_bin_path.c_str(), nginx_conf_path.c_str());
    if(!ret.first) {
        unlink(filepath);
        return ret;
    }
    return std::pair<bool, std::string>(true, "add successful.");
}
