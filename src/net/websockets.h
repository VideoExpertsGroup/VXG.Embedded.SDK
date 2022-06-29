#ifndef __WS_TRANSPORT
#define __WS_TRANSPORT

#include <libwebsockets.h>

#include "lws_common.h"
#include "transport.h"

namespace vxg {
namespace cloud {
namespace transport {

namespace libwebsockets {
class websocket : public lws_common {
    logger::logger_ptr logger = vxg::logger::instance("websockets-transport");

public:
    using ptr = std::shared_ptr<websocket>;

    websocket(bool allow_invalid_ssl = false);
    websocket(std::function<void(transport::Data&)> handler,
              bool allow_invalid_ssl = false);
    virtual ~websocket() {}

    virtual bool start() override;
    virtual bool stop() override;
    virtual bool connect(std::string uri);
    virtual bool disconnect();

    virtual void* poll_() override;

    static int lws_callback_(struct lws* wsi,
                             enum lws_callback_reasons reason,
                             void* user,
                             void* in,
                             size_t len);

    virtual bool send(const transport::Data& message);
    virtual bool send(const transport::Message& message);

    bool is_secure() { return ssl_; }

    void set_connected_cb(std::function<void()> cb) { connected_ = cb; }

    void set_disconnected_cb(std::function<void()> cb) {
        logger->debug("Set on_disconnected callback to {}",
                      (cb == nullptr ? "nullptr" : "not nullptr"));
        disconnected_ = cb;
    }

    void set_error_cb(std::function<void(void*, std::string error)> cb) {
        error_ = cb;
    }

    void set_message_cb(std::function<void(transport::Message&)> cb) {
        message_ = cb;
    }

    void set_proxy(std::string proxy) { proxy_ = proxy; }
private:
    std::string url_;
    std::string host_;
    uint16_t port_;
    bool ssl_ {false};
    std::string path_;
    bool close_connection_;
    bool allow_invalid_ssl_ {true};
    std::queue<std::string> requests_q_;
    std::mutex requests_q_lock_;
    std::string proxy_;

    std::function<void()> connected_;
    std::function<void()> disconnected_;
    std::function<void(void*, std::string error)> error_;
    std::function<void(transport::Message&)> message_;

    struct lws* __connect(std::string uri);

};  // class websocket
}  // namespace libwebsockets
}  // namespace transport
}  // namespace cloud
}  // namespace vxg

#endif
