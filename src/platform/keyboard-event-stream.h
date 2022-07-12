#pragma once

#include <agent/event-stream.h>

namespace vxg {
namespace cloud {
class keyboard_event_stream : public vxg::cloud::agent::event_stream {
    static constexpr const char* TAG = "kbd-event-stream";
    static constexpr const char* KEYBOARD_COMMAND_M = "m";
    static constexpr const char* KEYBOARD_COMMAND_M_VXG_EVENT =
        "keyboard-cmd-m";
    static constexpr int SLEEP_TIMEOUT_US = 100000;
    vxg::logger::logger_ptr logger {vxg::logger::instance(TAG)};

    // Thread
    std::thread kb_thread_;
    std::atomic<bool> running_ {false};
    std::mutex lock_;

    // Mapping
    struct kb_to_vxg_event_mapping {
        std::string kb_cmd;
        vxg::cloud::agent::proto::event_config vxg_event_conf;
        bool state;
    };
    // VXG event name -> mapping
    std::map<std::string, kb_to_vxg_event_mapping> mappings_;

    // Nonblocking keyboard reader
    static bool __getline_async(std::istream& is,
                                std::string& str,
                                char delim = '\n') {
        static std::string line_so_far;
        char in_ch;
        int chars_read = 0;
        bool line_read = false;
        str = "";

        do {
            chars_read = is.readsome(&in_ch, 1);
            if (chars_read == 1) {
                // if the delimiter is read then return the string so far
                if (in_ch == delim) {
                    str = line_so_far;
                    line_so_far = "";
                    line_read = true;
                } else {  // otherwise add it to the string so far
                    line_so_far.append(1, in_ch);
                }
            }
        } while (chars_read != 0 && !line_read);

        return line_read;
    }

    bool _get_mapping_by_cmd(const std::string& cmd,
                             kb_to_vxg_event_mapping& mapping) {
        // Search for mapping by the keyboard cmd
        auto _mapping = std::find_if(
            mappings_.begin(), mappings_.end(),
            [cmd](const std::pair<std::string, kb_to_vxg_event_mapping>& m) {
                return m.second.kb_cmd == cmd;
            });

        if (_mapping != mappings_.end()) {
            mapping = _mapping->second;
            return true;
        }

        return false;
    }

    void _toggle_mapping_state_by_cmd(const std::string& cmd) {
        // Search for mapping by the keyboard cmd
        auto _mapping = std::find_if(
            mappings_.begin(), mappings_.end(),
            [cmd](const std::pair<std::string, kb_to_vxg_event_mapping>& m) {
                return m.second.kb_cmd == cmd;
            });

        if (_mapping != mappings_.end()) {
            // Toggle state of statefull event
            if (_mapping->second.vxg_event_conf.caps.stateful)
                _mapping->second.state = !_mapping->second.state;
        }
    }

    static vxg::cloud::agent::proto::event_object _event_from_mapping(
        const kb_to_vxg_event_mapping& mapping) {
        vxg::cloud::agent::proto::event_object result;

        result.event = mapping.vxg_event_conf.event;
        result.custom_event_name = mapping.vxg_event_conf.custom_event_name;
        result.time = utils::time::to_double(utils::time::now());
        result.active = mapping.vxg_event_conf.caps.stateful && mapping.state;

        return result;
    }

    static bool input_avail() {
        struct timeval tv;
        fd_set fds;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        // Check if select() has failed, it may happen if the process was
        // daemonized and stdin was closed, we sleep in that case, otherwise we
        // will fall into the busy loop consuming whole CPU time.
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) < 0)
            usleep(SLEEP_TIMEOUT_US);
        return (FD_ISSET(0, &fds));
    }

    static int interrupt_;
    static std::string _get_cmd() {
        std::string cmd;

        fcntl(STDIN_FILENO, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);

        while (!interrupt_) {
            if (input_avail()) {
                char ch, ret;
                if (1 == (ret = read(STDIN_FILENO, &ch, 1))) {
                    if (ch == '\n')
                        break;
                    cmd += ch;
                } else {
                    usleep(SLEEP_TIMEOUT_US);
                }
            }
        }

        fcntl(0, F_SETFL, fcntl(0, F_GETFL) & ~O_NONBLOCK);
        if (interrupt_)
            return "";
        return cmd;
    }

    static void __keyboard_routine(keyboard_event_stream* _this) {
        vxg::cloud::utils::set_thread_name("kbd");

        while (_this->running_) {
            std::string cmd = _get_cmd();

            if (!cmd.empty()) {
                kb_to_vxg_event_mapping m;

                _this->_toggle_mapping_state_by_cmd(cmd);

                if (_this->_get_mapping_by_cmd(cmd, m)) {
                    // Construct event with the mapping and notify
                    _this->notify(_event_from_mapping(m));
                }
            } else
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    void _start_kb_thread() {
        std::lock_guard<std::mutex> lock(lock_);

        if (running_) {
            logger->warn("Starting not stopped kb_thread");
            return;
        }

        running_ = true;
        interrupt_ = 0;
        kb_thread_ = std::thread(__keyboard_routine, this);
    }

    void _stop_kb_thread() {
        std::lock_guard<std::mutex> lock(lock_);

        running_ = false;
        interrupt_ = 1;

        if (kb_thread_.joinable())
            kb_thread_.join();
        else {
            logger->warn("Can't stop kb thread, not joinable.");

            if (std::this_thread::get_id() == kb_thread_.get_id())
                kb_thread_.detach();
        }
    }

public:
    keyboard_event_stream() : event_stream("kbd-event-stream") {
        vxg::cloud::agent::proto::event_config conf;
        conf.event = vxg::cloud::agent::proto::ET_CUSTOM;
        conf.custom_event_name = KEYBOARD_COMMAND_M_VXG_EVENT;
        conf.active = true;
        conf.stream = true;
        conf.snapshot = true;
        conf.caps.periodic = false;
        conf.caps.snapshot = true;
        conf.caps.stateful = true;
        conf.caps.stream = true;
        conf.caps.trigger = true;

        mappings_[KEYBOARD_COMMAND_M_VXG_EVENT] = {"m", conf, false};
    }
    virtual ~keyboard_event_stream() {}

    virtual bool get_events(std::vector<vxg::cloud::agent::proto::event_config>&
                                events_configs) override {
        for (auto& mapping : mappings_) {
            // Search for our mappings in the already reported configs
            auto ec = std::find_if(
                events_configs.begin(), events_configs.end(),
                [mapping](const vxg::cloud::agent::proto::event_config& ec) {
                    return mapping.second.vxg_event_conf.name_eq(ec);
                });

            // If this mapping config is not yet reported add it to the configs
            // list
            if (ec == events_configs.end())
                events_configs.push_back(mapping.second.vxg_event_conf);
        }
        return true;
    }

    virtual bool start() override {
        // Notify all states of the mapped events on start before the thread
        // start to prevent concurent notifications
        for (auto& m : mappings_) {
            notify(_event_from_mapping(m.second));
        }

        // Start keyboard commands reader thread
        _start_kb_thread();

        logger->info("Keyboard event stream started");

        return true;
    }

    virtual void stop() override {
        // Start keyboard commands reader thread
        _stop_kb_thread();

        logger->info("Keyboard event stream stopped");
    }

    // Dummy overloading of the methods below

    virtual bool set_events(
        const std::vector<vxg::cloud::agent::proto::event_config>& conf) {
        return true;
    }

    virtual bool set_trigger_recording(bool, int, int) { return false; }

    virtual bool init() { return true; }

    virtual void finit() {}
};
int keyboard_event_stream::interrupt_ = 0;
}  // namespace cloud
}  // namespace vxg