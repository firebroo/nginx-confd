#ifndef _NGINX_CONF_PARSE_H
#define _NGINX_CONF_PARSE_H

#include "util.h"

#include <string>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

using namespace boost;
using namespace std;

//void parse(std::string& nginx_conf);
class nginxConfParse {
    private:
        std::string content;
        filesystem::path nginx_conf_path;
    public:
        nginxConfParse(){};
        nginxConfParse(const char* nginx_conf_path){
            this->nginx_conf_path = filesystem::path(nginx_conf_path); 
        }

        std::unordered_map<std::string, vector<std::string>> parse(const char* nginx_conf_path) {
            std::unordered_map<std::string, vector<std::string>> ss;

            this->nginx_conf_path = filesystem::path(nginx_conf_path); 
            this->content = this->load_conf_content(this->nginx_conf_path.string());
            this->content = this->remove_comment_and_space_line(this->content);
            this->load_include_conf();
            unordered_map<std::string, vector<std::string>> upstreams = this->extract_upstream_struct(this->content);
            unordered_map<std::string, std::string> servers = this->extract_server_struct(this->content);
            for (auto& server: servers) {
                string proxy_pass(server.second);
                if (upstreams.count(proxy_pass) > 0) {
                    ss[server.first] = upstreams[proxy_pass];
                } else {
                    ss[server.first] = vector<string>(1, server.second);
                }
            }
            return ss;
        }

        

    private:
        unordered_map<string, vector<string>> extract_upstream_struct(const std::string& content) {
            unordered_map<string, vector<string>> upstreams;
            regex e("upstream\\s+(.*?)\\{(.*?)\\}", boost::regex::perl|boost::regex::mod_s);   
            boost::sregex_iterator end;
            boost::sregex_iterator iter(content.begin(), content.end(), e);
            for (; iter != end; iter++) {
                string upstream_name((*iter)[1]);
                string upstream_servers((*iter)[2]);
                regex e2("server\\s+(.*?)(\\s+|;)", boost::regex::perl|boost::regex::no_mod_s);
                boost::sregex_iterator end2;
                boost::sregex_iterator iter2(upstream_servers.begin(), upstream_servers.end(), e2);
                vector<string> vtmp;
                for (; iter2 != end2; iter2++) {
                    string s((*iter2)[1]);
                    vtmp.push_back(trim(s));
            
                }
                upstreams[trim(upstream_name)] = vtmp; 
            }

            return upstreams;
        }

        unordered_map<std::string, std::string> extract_server_struct(const std::string& content) {
            unordered_map<string, string> servers;

            boost::sregex_iterator end;

            regex e("server\\s*?\\{.*?\\}", boost::regex::perl|boost::regex::mod_s);
            boost::sregex_iterator iter(content.begin(), content.end(), e);
            for (; iter != end; iter++) {
                string server((*iter)[0]);
                boost::smatch listen_what;
                boost::smatch server_name_what;
                boost::smatch proxy_pass_what;
                regex listen_e("^\\s*?listen\\s+(.*?);", boost::regex::perl|boost::regex::no_mod_s);
                regex server_name_e("^\\s*server_name\\s+(.*?);", boost::regex::perl|boost::regex::no_mod_s);
                regex proxy_pass_e("location\\s+/\\s+\\{.*?proxy_pass(.*?);.*?\\}", boost::regex::perl|boost::regex::mod_s);

                std::string listen, server_name, proxy_pass;
                if (boost::regex_search(server, listen_what, listen_e)) {
                    listen = string(listen_what[1]);
                    listen = trim(listen);
                } else {
                    continue;
                }

                if (boost::regex_search(server, server_name_what, server_name_e)) {
                    server_name = string(server_name_what[1]);
                    server_name = trim(server_name);
                } else {
                    continue;
                }

                if (boost::regex_search(server, proxy_pass_what, proxy_pass_e)) {
                    proxy_pass = string(proxy_pass_what[1]);
                    proxy_pass = this->remove_protocol(trim(proxy_pass));
                }

                //multi value server_name
                std::vector<std::string> strs;
                boost::split(strs, server_name, boost::is_any_of(" "));
                for (size_t i = 0; i < strs.size(); i++) {
                    string ele = trim(strs[i]);
                    if (!ele.empty()) {
                        std::vector<std::string> listens;
                        boost::split(listens, listen, boost::is_any_of(" "));
                        for (size_t i = 0; i < listens.size(); i++) {
                            string port = trim(listens[i]);
                            if (!port.empty() && !is_nginx_listen_opt(port)) {
                                string key = ele + ":" + port;
                                servers[key] = proxy_pass; 
                            }
                        }
                    }
                }
            }

            return servers; 
        }

        bool is_nginx_listen_opt(std::string port) {
            static const std::string listen_opt[] = {"default_server", "proxy_protocol", "ssl", "http2", "reuseport"};
            static const size_t listen_opt_count = sizeof(listen_opt)/sizeof(listen_opt[0]);
            for (size_t  i = 0; i < listen_opt_count; i++) {
                if (listen_opt[i] == port) {
                    return true;
                }
            } 
            return false;
        }
        
        std::string remove_protocol(std::string str) {
            std::size_t pos = str.find_first_of(":");
            if (pos != std::string::npos) {
                return str.substr(pos+strlen("://"));
            }  
            return str;
        } 

        void load_include_conf(){
            boost::sregex_iterator end;
            unordered_map<string, string> include;

            regex e("^(\\s*?include\\s+(.*?)\\*(\\.conf);)", boost::regex::perl|boost::regex::no_mod_s);
            boost::sregex_iterator iter(this->content.begin(), this->content.end(), e);
            for (; iter != end; iter++) {
                std::string path((*iter)[2]);
                if (path.c_str()[0] != '/') {
                    path = this->nginx_conf_path.parent_path().string() + "/" + path;
                }
                if (!filesystem::is_directory(path)) {
                    continue;
                }
                filesystem::recursive_directory_iterator beg_iter, end_iter;
                string all_content;
                for (beg_iter = filesystem::recursive_directory_iterator(path); beg_iter != end_iter; ++beg_iter) {
                    if (filesystem::is_directory(*beg_iter)) {
                        continue;
                    } 
                    if (beg_iter->path().extension().string() == (*iter)[3]) {
                        std::string str_path = beg_iter->path().string();
                        string content = load_conf_content(str_path);
                        all_content = all_content + content;
                    }
                }
                all_content = this->remove_comment_and_space_line(all_content);
                include[(*iter)[1]] = all_content;
            }
            for(auto& iter: include) {
                this->content = replace_all(this->content, iter.first, iter.second);
            }
        }

        string load_conf_content(string path){
            ifstream ifs; 
            ifs.open(path);
            if (!ifs) {
                return string("");
            }

            ostringstream os;
            os << ifs.rdbuf();
            ifs.close();
            return os.str();
        }
        
        string remove_comment_and_space_line(string& content){
            regex e("^\\s*?#.*?\\n|^\\s*?\\n", boost::regex::perl|boost::regex::no_mod_s);
            return regex_replace(content, e, ""); 
        }
        
};

#endif
