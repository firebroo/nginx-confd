#include "log.h"
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

namespace logging = boost::log;

void init_log(int verbosity, const std::string& log_file) {
    using namespace logging;
    using namespace logging::trivial;
    logging::register_simple_formatter_factory<severity_level, char>("Severity");
    logging::add_common_attributes();
    std::string log_format = "[%TimeStamp%] [%Severity%] %Message%";
    logging::add_file_log(
        keywords::file_name = log_file,
        //keywords::rotation_size = 100L * 1024 * 1024,
        keywords::format = log_format,
        keywords::auto_flush = true,
        keywords::filter = severity >= verbosity,
        boost::log::keywords::open_mode = ( std::ios::out | std::ios::app)
    );

}



/*
int main(int, char*[])
{
    init_log(logging::trivial::info, std::string("/home/traffic/traffic.log"));
    BOOST_LOG_TRIVIAL(trace) << "A trace severity message";
    BOOST_LOG_TRIVIAL(debug) << "A debug severity message";
    BOOST_LOG_TRIVIAL(info) << "An informational severity message";
    BOOST_LOG_TRIVIAL(warning) << "A warning severity message";
    BOOST_LOG_TRIVIAL(error) << "An error severity message";
    BOOST_LOG_TRIVIAL(fatal) << "A fatal severity message";


    return 0;
}
*/
