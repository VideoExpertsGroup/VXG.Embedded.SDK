#include "logging.h"

namespace vxg {

std::string logger::log_pattern_ {
    "%Y:%m:%d %H:%M:%S.%e [%=20n] [%=6t] %^%=7l%$ %v"};
std::string logger::syslog_ident_;
std::string logger::logfile_path_;
std::string logger::crash_logfile_path_;
size_t logger::logfile_max_size_;
size_t logger::logfile_max_files_;
bool enable_syslog_ = true;
logger::loglevel logger::default_loglevel_ = logger::lvl_trace;
bool logger::tcp_logsink_enabled_ {false};
std::string logger::tcp_logsink_host_;
uint16_t logger::tcp_logsink_port_;

std::shared_ptr<spdlog::sinks::dist_sink_mt> logger::dist_sink_;
std::shared_ptr<spdlog::sinks::syslog_sink_mt> logger::syslog_sink_;
std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> logger::console_sink_;
std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> logger::file_sink_;
std::shared_ptr<spdlog::sinks::tcp_sink_mt> logger::tcp_sink_;

}  // namespace vxg