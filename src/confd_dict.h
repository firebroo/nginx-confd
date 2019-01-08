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
      std::pair<bool, std::string> add_item(std::string listen_port, std::string domain, 
                std::vector<std::string> value, std::string tmp_style, bool force, HEALTH_CHECK type);
      bool shm_sync_to_dict();
      bool dict_sync_to_shm();
      std::string json_stringify();
      std::pair<bool, std::vector<std::string>> get_value_by_key(std::string key);
      std::pair<bool, std::string> delete_key(std::string key);
      void update_status(bool status);
      bool update_status(bool status, u_int solt);
      bool status(u_int solt);
    private:
      unordered_map<std::string, std::vector<std::string>> ss;
};

#endif
