#include "util.h"
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
processname_rename(char *p, const char* wanted_name)
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
    static const std::string units[] = {"k", "kb", "m", "mb", "g", "gb", "t", "tb"};
    static const size_t factors[] = {1024, 1024, 
        1024 * 1024, 1024 * 1024, 
        1024 * 1024 * 1024, 1024 * 1024 * 1024, 
        1024ULL * 1024 * 1024 * 1024, 1024ULL * 1024 * 1024 * 1024};
    const size_t unit_count = sizeof(units) / sizeof(units[0]);
    
    std::string str_num = boost::algorithm::trim_copy(str);
    size_t scale_factor = 1;
    for (size_t i = 0; i < unit_count; ++i) {
        if (boost::algorithm::iends_with(str_num, units[i])) {
            str_num = str_num.substr(0, str_num.size() - units[i].size());
            boost::algorithm::trim(str_num);
            scale_factor = factors[i];
            break;
        }
    }
    char* pend = NULL;
    size_t num = strtoul(str_num.c_str(), &pend, 0);
    if (pend == NULL || *pend != '\0') {
        throw std::invalid_argument("Invalid bytes number: " + str);
    }
    return num * scale_factor;
}

