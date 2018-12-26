#include "log.h"
#include "confd_dict.h"
#include "confd_shm.h"
#include "confd_shmtx.h"
#include "../lib/jsoncpp/src/json/json.h"

extern confd_shm_t     *shm;
extern confd_shmtx_t   *shmtx;
extern std::unordered_map<std::string, std::string> confd_config;

std::pair<bool, std::string> 
confd_dict::add_key(std::string listen_port, std::string domain, 
          std::vector<std::string> value, std::string tmp_style)
{
    string key = domain + ":" + listen_port; 
    if (this->ss.count(key) > 0 ) {
        string error = "domain(" + domain + ") listen port("+listen_port+") is exists.";
        return std::pair<bool, std::string>(false, error);
    }
    this->ss[key] = value;
    auto ret = nginx_opt::sync_to_disk(listen_port, domain, value, tmp_style, \
        confd_config["nginx_conf_writen_path"], confd_config["nginx_bin_path"], confd_config["nginx_conf_path"]);
    if (ret.first) {
        this->dict_sync_to_shm();
    }
    return ret;
}

bool 
confd_dict::shm_sync_to_dict()
{
    Json::CharReaderBuilder builder;
    Json::CharReader* reader = builder.newCharReader();
    Json::Value json_root;
    std::string errors;
    
    size_t str_count = strlen(shm->addr);
    char* data_cloned = (char*)malloc(str_count);
    lock(shmtx);
    strncpy(data_cloned, shm->addr, str_count);
    unlock(shmtx);
    
    if (reader->parse(data_cloned, data_cloned + str_count, &json_root, &errors)) {
        Json::Value::Members members;  
        members = json_root.getMemberNames(); 
        for (Json::Value::Members::iterator iterMember = members.begin(); iterMember != members.end(); iterMember++){
            std::string key(*iterMember); 
            Json::Value json_value(json_root[key]);
            vector<string> vcs;
            for(u_int j = 0; j < json_value.size(); j++) {
                vcs.push_back(json_value[j].asString());
            }
            this->ss[key] = vcs;
        }
    } else {
        BOOST_LOG_TRIVIAL(warning) << "Parse json str failed error: " << errors;
    }

    free(data_cloned);
    delete reader;

    return true;
}


bool 
confd_dict::dict_sync_to_shm()
{
    string new_data = this->json_stringify();
    lock(shmtx);
    strcpy(shm->addr, new_data.c_str());
    unlock(shmtx);
    return true;
}

std::string 
confd_dict::json_stringify()
{
    Json::Value root;
    for (auto &iter: this->ss) {
        vector<string>& vcs(iter.second);
        for (auto &it: vcs) {
            root[iter.first].append(it);
        }
    } 
    return root.toStyledString();
} 

std::pair<bool, std::vector<std::string>>
confd_dict::get_value_by_key(std::string key)
{
    if (this->ss.count(key) > 0 ) {
        return std::pair<bool, std::vector<std::string>>(true, this->ss[key]);
    }
    return std::pair<bool, std::vector<std::string>>(false, std::vector<std::string>());
}
