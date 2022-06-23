#pragma once

#include <libwebsockets.h>

#include "lws_common.h"
#include "transport.h"

namespace vxg {
namespace cloud {
namespace transport {

namespace libwebsockets {
class sshd : public lws_common {
    logger::logger_ptr logger = vxg::logger::instance("sshd-transport");

public:
    using ptr = std::shared_ptr<sshd>;

    sshd(uint16_t port = 2200);
    virtual ~sshd();

    virtual bool start() override;
    virtual bool stop() override;

    virtual void* poll_() override;

private:
    uint16_t port_;
};  // class websocket
}  // namespace libwebsockets
}  // namespace transport
}  // namespace cloud
}  // namespace vxg
