#include "util.h"
#include <fstream> 
#include <vector> 
#include <iostream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdarg.h>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

std::string
ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
    return s;
}

std::string
rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    return s;
}

std::string
trim(std::string &s)
{
    ltrim(s);
    rtrim(s);
    return s;
}

char*
process_rename(char *p, const char* wanted_name)
{
    memset(p, '\0', strlen(wanted_name)+1);
    strcpy(p, wanted_name);
    return p;
}

std::string&
replace_all(std::string& str, const std::string& old_value, const std::string& new_value)
{   
    while (true){   
        std::string::size_type pos(0);   
        if (!((pos = str.find(old_value)) != std::string::npos)) {
            break;
        } 
        str.replace(pos, old_value.length(), new_value);   
    }   
    return str;   
}

size_t 
parse_bytes_number(const std::string& str)
{ 
    typedef struct units_map {
        std::string unit;
        size_t      size;
    } units_map_t;

    units_map_t units_map[] = {
        {"k",  1024},
        {"kb", 1024},
        {"m",  1024*1024},
        {"mb", 1024*1024},
        {"g",  1024*1024*1024},
        {"gb", 1024*1024*1024},
        {"t",  1024L*1024*1024*1024},
        {"tb", 1024L*1024*1024*1024},
        {"",   0L}
    };
    
    units_map_t* unit_item;
    std::string str_num = boost::algorithm::trim_copy(str);
    size_t scale_factor = 1;
    for (unit_item = units_map; unit_item->unit != ""; unit_item++) {
        if (boost::algorithm::iends_with(str_num, unit_item->unit)) {
            str_num = str_num.substr(0, str_num.size() - unit_item->unit.size());
            scale_factor = unit_item->size;
            break;
        }
    }
    char* pend = NULL;
    boost::algorithm::trim(str_num);
    size_t num = strtoul(str_num.c_str(), &pend, 0);
    if (pend == NULL || *pend != '\0') {
        throw std::invalid_argument("Invalid bytes number: " + str);
    }
    return num * scale_factor;
}

long
get_meminfo_kv(const char* k)
{
    long size = -1;
    std::string line;

    std::ifstream meminfo_file("/proc/meminfo");     
    if (!meminfo_file.is_open()) { 
        return size;
    }
    boost::smatch what;
    
    while (std::getline(meminfo_file, line)) {
        if (!boost::algorithm::starts_with(line, k)) {
            continue;
        }
        boost::regex reg(":\\s+(\\d+)(.*)", boost::regex::perl|boost::regex::no_mod_s);
        if (boost::regex_search(line, what, reg)) {
            std::string size_str(what[1]);
            std::string unit(what[2]);
            size_str = trim(size_str) + boost::algorithm::to_lower_copy(trim(unit));
            size = parse_bytes_number(size_str);            
        }
    } 
    meminfo_file.close();
    return size;
}

long
get_MemAvailable()
{
    return get_meminfo_kv("MemAvailable");    
}

std::pair<bool, std::string>
exec_cmd(const char* cmd)
{
    char buf[10240] = {0};
    std::string stdout_buffer;

    FILE* fp = popen(cmd, "r");
    if (!fp) {
        return std::pair<bool, std::string>(false, strerror(errno));
    }
    while (!feof(fp)) {
        if (fgets(buf, sizeof(buf), fp) != NULL) {
            stdout_buffer += std::string(buf);
        }
    }
    if (pclose(fp) == -1) {
        return std::pair<bool, std::string>(false, strerror(errno));
    }

    return std::pair<bool, std::string>(true, stdout_buffer);
}


