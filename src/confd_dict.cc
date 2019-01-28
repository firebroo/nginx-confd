#include "log.h"
#include "confd_dict.h"
#include "confd_shm.h"
#include "confd_shmtx.h"
#include "json.h"

extern confd_shm_t     *shm;
extern confd_shmtx_t   *shmtx;
extern confd_shm_t     *update;
extern confd_shmtx_t   *updatetx;
extern std::unordered_map<std::string, std::string> confd_config;

std::pair<bool, std::string> 
confd_dict::add_item(std::string listen_port, std::string domain, 
          std::vector<std::string> value, std::string tmp_style, bool force, HEALTH_CHECK type)
{
    std::string key = domain + "_" + listen_port; 
    std::string file_path(confd_config["nginx_conf_writen_path"] + key + ".conf");

    if (force) { //强制覆盖
        bool ret = nginx_opt::backup_single_conf(file_path);
        if (!ret) {
            BOOST_LOG_TRIVIAL(debug) << "backup file " << file_path << " failed.";
            return std::pair<bool, std::string>(false, "backup file failed.");
        }
        BOOST_LOG_TRIVIAL(debug) << "backup file " << file_path << " successful.";
    } else {
        if (this->ss.count(key) > 0 ) {
            std::string error = "domain(" + domain + ") listen port("+listen_port+") is exists.";
            return std::pair<bool, std::string>(false, error);
        }
    }
    std::pair<bool, std::string> ret = nginx_opt::sync_to_disk(listen_port, domain, value, tmp_style, \
                                       confd_config["nginx_conf_writen_path"], confd_config["nginx_bin_path"], \
                                       confd_config["nginx_conf_path"], type);
    if (!ret.first) {
        if (force) {
            bool ret = nginx_opt::rollback_single_conf(file_path);
            if (!ret) {
                BOOST_LOG_TRIVIAL(debug) << "rollback file " << file_path << " failed.";
                return std::pair<bool, std::string>(false, "rollback file failed.");
            }
            BOOST_LOG_TRIVIAL(debug) << "rollback file " << file_path << " successful.";
        }
    } else {
        this->ss[key] = value;
        this->dict_sync_to_shm();
        this->update_status(true);
    }

    return ret;
}

bool 
confd_dict::status(u_int solt)
{
    if (solt > update->size) {
        BOOST_LOG_TRIVIAL(warning) << "status(" << solt << ") solt is large than " << update->size;
        return false;
    }
    bool status;
    lock(updatetx);  
    status = (bool)(*((update->addr)+solt));
    unlock(updatetx);  

    return status;
}

void
confd_dict::update_status(bool status)
{
    lock(updatetx);  
    memset(update->addr, status, update->size);
    unlock(updatetx);  
}

bool
confd_dict::update_status(bool status, u_int solt)
{
    if (solt > update->size) {
        return false;
        BOOST_LOG_TRIVIAL(warning) << "update_status(" << status << "," << solt << ") solt is large than " << update->size;
    }
    lock(updatetx);  
    *((update->addr)+solt) = status;
    unlock(updatetx);  

    return true;
}

std::pair<bool, std::string> 
confd_dict::delete_key(std::string key)
{
    if (this->ss.count(key) == 0 ) {
        return std::pair<bool, std::string>(false, "domain(" + key + ") doesn't exists.");
    }  
    std::pair<bool, std::string> ret = nginx_opt::delete_conf(key, confd_config["nginx_conf_writen_path"]);
    if (!ret.first) {
        BOOST_LOG_TRIVIAL(info) << ret.second;
        return ret;
    }
    if (this->ss.erase(key) <= 0) {  //删除key
        BOOST_LOG_TRIVIAL(info) << "delete dict key(" + key + ") failed.";
        return std::pair<bool, std::string>(false, "delete dict key(" + key + ") failed.");
    }      
    this->dict_sync_to_shm();       //同步到shm内存
    this->update_status(true);
    BOOST_LOG_TRIVIAL(info) << ret.second;
    BOOST_LOG_TRIVIAL(info) << "delete domain(" << key << ") successful.";
    return std::pair<bool, std::string>(true, "delete successful.");
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
