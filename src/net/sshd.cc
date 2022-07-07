#include "sshd.h"
#include "lws-sshd.h"

namespace vxg {
namespace cloud {
namespace transport {
namespace libwebsockets {

sshd::sshd(uint16_t port) : lws_common(nullptr), port_ {port} {}
sshd::~sshd() {
    stop();
}

bool sshd::start() {
    lws_context_ = lws_create_sshd_context();

    if (!lws_context_)
        goto bail;

    return run();
bail:
    lws_context_destroy(lws_context_);
    lws_context_ = nullptr;

    return false;
}

bool sshd::stop() {
    /* blocked stopping polling thread */
    worker::term();
    /* cleanup all timers */
    lws_common::stop();

    if (lws_context_)
        lws_context_destroy(lws_context_);

    lws_context_ = nullptr;

    return true;
}

void* sshd::poll_() {
    ssize_t n = 0;
    utils::set_thread_name("sshd");
    while (n >= 0 && lws_context_ && running_)
        n = lws_service(lws_context_, 100);

    logger->info("sshd polling thread stopped");

    return nullptr;
}

}  // namespace libwebsockets
}  // namespace transport
}  // namespace cloud
}  // namespace vxg