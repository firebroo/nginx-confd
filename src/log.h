#ifndef _LOG_H
#define _LOG_H

#include <boost/log/trivial.hpp>

void init_log(int verbosity, const std::string& log_file);

#endif
