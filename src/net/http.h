#ifndef __HTTP_TRANSPORT
#define __HTTP_TRANSPORT

#include <condition_variable>  // std::condition_variable
#include <mutex>               // std::mutex, std::unique_lock

#include <libwebsockets.h>
#include <chrono>

#include "lws_common.h"
#include "transport.h"

namespace vxg {
namespace cloud {
namespace transport {
namespace libwebsockets {
class http : public std::enable_shared_from_this<http>, public lws_common {
    logger::logger_ptr logger = vxg::logger::instance("http-transport");

public:
    typedef std::shared_ptr<http> http_ptr;
    typedef std::shared_ptr<http> ptr;
    struct request : std::enable_shared_from_this<request> {
        /* clang-format off */
        typedef enum {
            AUTH_NONE,
            AUTH_BASIC,
            AUTH_DIGEST,
            AUTH_ANY,
        } auth_type;
        /* clang-format on */
        struct auth_context {
            auth_type type;
            std::string username;
            std::string password;

            auth_context(auth_type t = AUTH_NONE,
                         std::string u = "",
                         std::string p = "")
                : type {t}, username {u}, password {p} {}

            std::string auth_type_string() {
                std::string result = "unknown";
                switch (type) {
                    case http::request::AUTH_NONE:
                        return "none";
                    case http::request::AUTH_BASIC:
                        return "basic";
                    case http::request::AUTH_DIGEST:
                        return "digest";
                    case http::request::AUTH_ANY:
                        return "any";
                    default:
                        return "unknown";
                }
            }
        };

        request(std::string url,
                const std::string method = "GET",
                const std::string body = "",
                std::chrono::seconds timeout = std::chrono::seconds(30))
            : url_(url),
              method_(method),
              request_body_(body),
              timeout_s_(timeout) {}

        request(std::string url,
                const std::string method,
                const std::vector<uint8_t> body,
                std::chrono::seconds timeout = std::chrono::seconds(2 * 60))
            : url_(url), method_(method), timeout_s_(timeout) {
            request_body_.append((char*)body.data(), body.size());
        }

        int code() { return status_; }

        void stats_start() { time_start = std::chrono::system_clock::now(); }

        void stats_update() {
            time_now = std::chrono::system_clock::now();
            auto request_duration =
                std::chrono::duration_cast<std::chrono::seconds>(time_now -
                                                                 time_start)
                    .count();

            if (request_duration == 0)
                request_duration = 1;

            if (!request_body_.empty())
                upload_speed = send_pos / request_duration;

            if (!response_body_.empty())
                download_speed = send_pos / request_duration;
        }

        void stats_finalize() {
            time_stop = std::chrono::system_clock::now();
            auto request_duration =
                std::chrono::duration_cast<std::chrono::seconds>(time_stop -
                                                                 time_start)
                    .count();

            if (request_duration == 0)
                request_duration = 1;

            if (!request_body_.empty())
                upload_speed = request_body_.size() / request_duration;

            if (!response_body_.empty())
                download_speed = response_body_.size() / request_duration;
        }

        void set_auth_context(auth_context ctx) { auth_context_ = ctx; }

        void set_header(std::string key, std::string val) {
            key += ":";
            headers_[key] = val;
        }

        void notify_finish() {
            std::unique_lock<std::mutex> lock(lock_);
            cond_.notify_all();
        }

        std::function<void(transport::Data&, int status)> response_;
        void set_response_cb(
            std::function<void(transport::Data&, int status)> cb) {
            response_ = cb;
        }

        std::shared_ptr<request> get_shared_ptr() { return shared_from_this(); }

        std::map<std::string, std::string> headers_;
        std::string url_;
        std::string method_;
        std::string body_;
        bool multipart_ {false};
        std::string multipart_form_field_ {"file"};
        std::string multipart_upload_filename_ {"file"};
        std::string multipart_content_type_ {"application/octet-stream"};

        transport::Data response_body_;
        transport::Data request_body_;
        std::map<std::string, std::string> response_headers_;
        size_t send_pos {0};

        std::chrono::time_point<std::chrono::system_clock> time_start;
        std::chrono::time_point<std::chrono::system_clock> time_stop;
        std::chrono::time_point<std::chrono::system_clock> time_now;
        size_t download_speed {0};
        size_t upload_speed {0};
        size_t max_upload_speed {0};

        struct delayed_on_writeable_payload {
            lws_sorted_usec_list_t sul;
            struct lws* wsi {nullptr};
        };
        struct delayed_on_writeable_payload delayed_on_write_payload;

        struct lws* client_wsi_ {nullptr};
        bool close_connection_ {false};

        std::chrono::seconds timeout_s_;
        int status_ {-1};
        bool bad_ {0};
        std::mutex lock_;
        std::condition_variable cond_;

        auth_context auth_context_;
    };
    typedef std::shared_ptr<request> request_ptr;

    http(bool allow_invalid_ssl = false);
    ~http() { stop(); };

    virtual bool start() override;
    virtual bool stop() override;
    void set_proxy(std::string proxy) { proxy_ = proxy; }
    virtual bool is_secure() override;

    virtual bool make(request_ptr req);
    virtual bool make_blocked(request_ptr req);

    bool get(request_ptr req);
    bool post(request_ptr req);

    virtual void* poll_() override;

    static int lws_callback_(struct lws* wsi,
                             enum lws_callback_reasons reason,
                             void* user,
                             void* in,
                             size_t len);

    bool _do_request(request_ptr req);

    std::shared_ptr<http> getptr() { return shared_from_this(); }

private:
    std::string url_;
    std::string host_;
    uint16_t port_;
    bool ssl_;
    bool allow_invalid_ssl_ {true};
    std::string path_;
    std::queue<request_ptr> requests_q_;
    std::mutex requests_q_lock_;
    std::string proxy_;
};

}  // namespace libwebsockets
}  // namespace transport
}  // namespace cloud
}  // namespace vxg

#endif
