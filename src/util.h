#ifndef _UTIL_H
#define _UTIL_H

#include <string>

std::string ltrim(std::string &s);
std::string rtrim(std::string &s);
std::string trim(std::string &s);
size_t parse_bytes_number(const std::string& str);
std::string& replace_all(std::string& str, const std::string& old_value, const std::string& new_value);
long get_meminfo_kv(const char* k);
long get_MemAvailable();
std::pair<bool, std::string> exec_cmd(const char* cmd);

#endif
