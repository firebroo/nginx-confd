#ifndef _UTIL_H
#define _UTIL_H

#include <string>

std::string ltrim(std::string &s);
std::string rtrim(std::string &s);
std::string trim(std::string &s);
char* processname_rename(char *p, const char*wanted_name);
size_t parse_bytes_number(const std::string& str);
std::string& replace_all(std::string& str, const std::string& old_value, const std::string& new_value);

#endif
