#ifndef _MEM_DICT_H
#define _MEM_DICT_H

#include "config.h"
#include "nginx_opt.h"


class confd_dict {
    public:
      confd_dict(){};
      confd_dict(unordered_map<std::string, vector<std::string>> ss){
          this->ss = ss;
      }
      std::pair<bool, std::string> add_key(std::string listen_port, std::string domain, 
                std::vector<std::string> value, std::string tmp_style);
      bool shm_sync_to_dict();
      bool dict_sync_to_shm();
      std::string json_stringify();
      std::pair<bool, std::vector<std::string>> get_value_by_key(std::string key);
    private:
      unordered_map<std::string, std::vector<std::string>> ss;
};

#endif
