#ifndef _MEM_DICT_H
#define _MEM_DICT_H

#include "config.h"
#include "nginx_opt.h"

extern char* data;
extern int sem_id;
extern unordered_map<string,string> confd_config;

class confd_dict {
    public:
      confd_dict(){};
      confd_dict(unordered_map<std::string, vector<std::string>> ss){
          this->ss = ss;
      }
      std::pair<bool, std::string> add_key(std::string listen_port, std::string domain, 
                std::vector<std::string> value, std::string tmp_style) {
          string key = domain + ":" + listen_port; 
          if (this->ss.count(key) > 0 ) {
              return std::pair<bool, std::string>(false, "domain(" + domain + ") is exists.");
          } else {
              this->ss[key] = value;
              auto ret = nginx_opt::sync_to_disk(listen_port, domain, value, tmp_style, confd_config["nginx_conf_writen_path"], confd_config["nginx_bin_path"], confd_config["nginx_conf_path"]);
              if (ret.first) {
                  this->sync_to_shm();
                  
              }
              return ret;
          }
      }

      bool shm_to_dict() {
          Json::CharReaderBuilder builder;
          Json::CharReader* reader = builder.newCharReader();
          Json::Value json_root;
          std::string errors;
          
          P(sem_id);
          if (reader->parse(data, data + strlen(data), &json_root, &errors)) {
              Json::Value::Members members;  
              members = json_root.getMemberNames(); 
              for (Json::Value::Members::iterator iterMember = members.begin(); iterMember != members.end(); iterMember++){
                  std::string key(*iterMember); 
                  Json::Value json_value(json_root[key]);
                  vector<string> vcs;
                  for(unsigned int j = 0; j < json_value.size(); j++) {
                      vcs.push_back(json_value[j].asString());
                  }
                  this->ss[key] = vcs;
              }
          } else {
              std::cout << "Parse json str failed error: " << errors << endl;;
          }
          V(sem_id);

          return true;
      }


      bool sync_to_shm() {
          string new_data = this->json_stringify();
          P(sem_id);
          strcpy(data, new_data.c_str());
          V(sem_id);
          return true;
      }
      std::string json_stringify() {
          Json::Value root;
          for (auto &iter: this->ss) {
              vector<string>& vcs(iter.second);
              for (auto &it: vcs) {
                  root[iter.first].append(it);
              }
          } 
          return root.toStyledString();
      } 
    private:
      unordered_map<std::string, vector<std::string>> ss;
};

#endif
