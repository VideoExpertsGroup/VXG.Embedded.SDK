#ifndef __LOGGING_H
#define __LOGGING_H

#include <spdlog/spdlog.h>

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/cfg/env.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/syslog_sink.h>
#include <spdlog/sinks/tcp_sink.h>

#include <utils/loguru.h>

#include <fstream>

namespace vxg {
//! @brief Logger class, current implementation based on spdlog
class logger {
public:
    typedef std::shared_ptr<spdlog::logger> logger_ptr;
    enum loglevel {
        lvl_crit = spdlog::level::critical,
        lvl_off = spdlog::level::off,
        lvl_error = spdlog::level::err,
        lvl_warn = spdlog::level::warn,
        lvl_info = spdlog::level::info,
        lvl_debug = spdlog::level::debug,
        lvl_trace = spdlog::level::trace,
    };

    struct options {
        std::string log_pattern {
            "%Y:%m:%d %H:%M:%S.%e [%=20n] [%=6t] %^%=7l%$ %v"};
        std::string logfile_path;
        size_t logfile_max_size;
        size_t logfile_max_files;
        std::string crash_logfile_path;
        std::string syslog_ident;
        loglevel default_loglevel {lvl_info};
        bool tcp_logsink_enabled {false};
        std::string tcp_logsink_host;
        uint16_t tcp_logsink_port;
    };

private:
    static std::string log_pattern_;
    static std::string logfile_path_;
    static size_t logfile_max_size_;
    static size_t logfile_max_files_;
    static std::string crash_logfile_path_;

    std::shared_ptr<spdlog::details::thread_pool> spdlog_thread_pool_;
    static std::string syslog_ident_;
    static loglevel default_loglevel_;

    static bool tcp_logsink_enabled_;
    static std::string tcp_logsink_host_;
    static uint16_t tcp_logsink_port_;

    static std::shared_ptr<spdlog::sinks::dist_sink_mt> dist_sink_;
    static std::shared_ptr<spdlog::sinks::syslog_sink_mt> syslog_sink_;
    static std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
    static std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> file_sink_;
    static std::shared_ptr<spdlog::sinks::tcp_sink_mt> tcp_sink_;

    static void _init_loguru_backtrace(int argc, char** argv) {
        loguru::shutdown();
        loguru::init(argc, argv);
        std::ifstream in(crash_logfile_path_,
                         std::ifstream::ate | std::ifstream::binary);
        size_t log_size = 0;
        struct stat stat_buf;

        if ((0 == ::stat(crash_logfile_path_.c_str(), &stat_buf)) &&
            log_size > logfile_max_size_) {
            vxg::logger::warn("Crash log is too big {:.3f} Mb, deleting.",
                              log_size / (double)(1024 * 1024));
            ::unlink(crash_logfile_path_.c_str());
        }

        if (!crash_logfile_path_.empty())
            loguru::add_file(crash_logfile_path_.c_str(), loguru::Append,
                             loguru::Verbosity_MAX);
    }

    static void _reset_sinks() {
        // DIST sink that is the master of all real sinks
        if (!dist_sink_)
            dist_sink_ = std::make_shared<spdlog::sinks::dist_sink_mt>();

        // SYSLOG sink
        if (syslog_sink_) {
            dist_sink_->remove_sink(syslog_sink_);
            syslog_sink_ = nullptr;
        }

        if (!syslog_ident_.empty()) {
            syslog_sink_ = std::make_shared<spdlog::sinks::syslog_sink_mt>(
                syslog_ident_, LOG_PID, 0, false);
            dist_sink_->add_sink(syslog_sink_);
        }

        // CONSOLE sink
        if (console_sink_) {
            dist_sink_->remove_sink(console_sink_);
            console_sink_ = nullptr;
        }

        // TCP sink
        if (tcp_sink_) {
            dist_sink_->remove_sink(tcp_sink_);
            tcp_sink_ = nullptr;
        }

        if (tcp_logsink_enabled_) {
            try {
                spdlog::sinks::tcp_sink_config cfg(tcp_logsink_host_,
                                                   tcp_logsink_port_);
                cfg.lazy_connect = true;
                tcp_sink_ = std::make_shared<spdlog::sinks::tcp_sink_mt>(cfg);
                dist_sink_->add_sink(tcp_sink_);
            } catch (std::exception& e) {
                info("Failed to create tcp sink: {}", e.what());
            }
        }
        console_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        dist_sink_->add_sink(console_sink_);

        // FILE sink
        if (file_sink_) {
            dist_sink_->remove_sink(file_sink_);
            file_sink_ = nullptr;
        }

        if (!logfile_path_.empty()) {
            file_sink_ = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                logfile_path_, logfile_max_size_, logfile_max_files_);
            dist_sink_->add_sink(file_sink_);
        }

        dist_sink_->set_pattern(log_pattern_);
        if (spdlog::details::os::getenv("SPDLOG_LEVEL").empty())
            dist_sink_->set_level((spdlog::level::level_enum)default_loglevel_);

        spdlog::cfg::load_env_levels();
    }

    //! @brief Construct a new logger object if it wasn't constructed already
    //!        Depending on what parameters were specified via logger::reset()
    //!        different logger sinks will be added to a new logger.
    //!
    //! @param logger_name Logger name. If logger with such name was already
    //!                    created, then it will be reused, otherwise a new one
    //!                    will be spawned.
    logger(std::string logger_name = "default") {
        auto default_logger = spdlog::get("default");

        if (default_logger == nullptr) {
            spdlog::init_thread_pool(8192, 1);
            spdlog_thread_pool_ = spdlog::thread_pool();

            // Init master dist_sink_ and real sinks depending on settings
            _reset_sinks();

            default_logger = std::make_shared<spdlog::async_logger>(
                "default", dist_sink_, spdlog_thread_pool_);
            default_logger->set_pattern(log_pattern_);
            default_logger->set_level(
                (spdlog::level::level_enum)default_loglevel_);

            spdlog::flush_every(std::chrono::seconds(1));
            spdlog::set_default_logger(default_logger);
        }

        spdlog::register_logger(default_logger->clone(logger_name));
        spdlog::cfg::load_env_levels();
    }

public:
    //! @brief Get pointer to the instance of the named spdlog::logger object.
    //!        On the very first call creates default logger named 'default'.
    //!        Contructs new logger if logger with such name was never requested
    //!
    //! @param[in] name Logger name. If logger with such name was already
    //!             created, then it will be reused, otherwise a new one
    //!             will be constructed.
    //! @return std::shared_ptr<spdlog::logger>
    static std::shared_ptr<spdlog::logger> instance(std::string name) {
        std::shared_ptr<spdlog::logger> l = spdlog::get(name);

        if (l == nullptr) {
            logger static_logger(name);
            l = spdlog::get(name);
        }

        return l;
    }

    //! @brief Reset default logger parameters. Used to change all loggers
    //!        parameters such as syslog/file sinks usage.
    //!        Should be called before very first logger::instance() call
    //!        to take effect.
    //!        If wasn't called the default console logging sink only will be
    //!        used for all loggers.
    //! @deprecated Use reset(const options& opts)
    //! @param argc Process argc
    //! @param argv Process argv
    //! @param l    default loglevel, all loggers will be created with this
    //!             loglevel, can be overriden with SPDLOG_LEVEL env variable
    //! @param syslog_ident Syslog identification string, if empty syslog
    //!                     logging will be disabled.
    //! @param logfile_path Rotating plain log file path, if empty no plain log
    //!                     file will be used.
    //! @param logfile_max_size Max log file size before invoking logrotate.
    //! @param logfile_max_files Max number if rotating logfiles.
    static void reset(int argc,
                      char** argv,
                      loglevel l,
                      std::string syslog_ident = "VXGCloudAgentDefault",
                      std::string crash_logfile_path = "",
                      std::string logfile_path = "",
                      size_t logfile_max_size = (1024 * 1024),
                      size_t logfile_max_files = 3) {
        default_loglevel_ = l;
        syslog_ident_ = syslog_ident;
        logfile_path_ = logfile_path;
        logfile_max_size_ = logfile_max_size;
        logfile_max_files_ = logfile_max_files;
        crash_logfile_path_ = crash_logfile_path;

        // _init_loguru_backtrace(argc, argv);
        _reset_sinks();
    }

    static void reset(const options& opts) {
        default_loglevel_ = opts.default_loglevel;
        syslog_ident_ = opts.syslog_ident;
        logfile_path_ = opts.logfile_path;
        logfile_max_size_ = opts.logfile_max_size;
        logfile_max_files_ = opts.logfile_max_files;
        crash_logfile_path_ = opts.crash_logfile_path;
        tcp_logsink_enabled_ = opts.tcp_logsink_enabled;
        tcp_logsink_host_ = opts.tcp_logsink_host;
        tcp_logsink_port_ = opts.tcp_logsink_port;

        // _init_loguru_backtrace(argc, argv);
        _reset_sinks();
    }

    //! @brief Change the logger object loglevel
    //!
    //! @param log_ptr Logger object pointer.
    //! @param lvl New loglevel.
    //!
    static void set_level(logger_ptr log_ptr, loglevel lvl) {
        log_ptr->set_level((spdlog::level::level_enum)lvl);
    }

    //! @brief Static info log
    //!
    //! @tparam FormatString
    //! @tparam Args
    //! @param fmt
    //! @param args
    template <typename FormatString, typename... Args>
    static void info(const FormatString& fmt, const Args&... args) {
        spdlog::info(fmt, args...);
    }
    template <typename FormatString, typename... Args>
    static void error(const FormatString& fmt, const Args&... args) {
        spdlog::error(fmt, args...);
    }
    template <typename FormatString, typename... Args>
    static void warn(const FormatString& fmt, const Args&... args) {
        spdlog::warn(fmt, args...);
    }
    template <typename FormatString, typename... Args>
    static void debug(const FormatString& fmt, const Args&... args) {
        spdlog::debug(fmt, args...);
    }
    template <typename FormatString, typename... Args>
    static void trace(const FormatString& fmt, const Args&... args) {
        spdlog::trace(fmt, args...);
    }
    template <typename T>
    static void trace(const T& msg) {
        spdlog::trace(msg);
    }

    template <typename T>
    static void debug(const T& msg) {
        spdlog::debug(msg);
    }

    template <typename T>
    static void info(const T& msg) {
        spdlog::info(msg);
    }

    template <typename T>
    static void warn(const T& msg) {
        spdlog::warn(msg);
    }

    template <typename T>
    static void error(const T& msg) {
        spdlog::error(msg);
    }
};

}  // namespace vxg
#endif  // __LOGGING_H