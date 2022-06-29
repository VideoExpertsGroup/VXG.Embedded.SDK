#include <cassert>

#include "websockets.h"

#include <config.h>

using namespace vxg::cloud::transport::libwebsockets;

/* clang-format off */
static const struct lws_protocols _lws_protocols[] = {
    {
        "vxg-message-protocol",
        websocket::lws_callback_,
        0,
        0,
    },
    {NULL, NULL, 0, 0}
};
/* clang-format on */

int websocket::lws_callback_(struct lws* wsi,
                             enum lws_callback_reasons reason,
                             void* user,
                             void* in,
                             size_t len) {
    websocket* ws = (websocket*)lws_context_user(lws_get_context(wsi));
    vxg::logger::logger_ptr logger = ws->logger;
    int ret = 0;

    logger->debug("{} {}", __func__, reason);

    switch (reason) {
        case LWS_CALLBACK_CLIENT_HTTP_REDIRECT: {
            logger->warn("Redirect to {}", (char*)in);
        } break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            logger->error("CLIENT_CONNECTION_ERROR: {}",
                          in ? (char*)in : "(null)");
            /* If we are in async receive mode we have to drain rx queue
             * before notifying about connection close
             */
            if (!ws->message_)
                ws->drain_rx();

            if (ws->error_)
                ws->error_(ws, in ? (char*)in : "(null)");
            // if (ws->disconnected_)
            //     ws->disconnected_();

            ws->client_wsi_ = nullptr;
            ws->close_connection_ = true;
            break;

        case LWS_CALLBACK_CLIENT_ESTABLISHED: {
            logger->info("Connection established");
            logger->debug("Connection wsi {}", (void*)wsi);

            ws->client_wsi_ = wsi;

            if (ws->connected_)
                ws->connected_();
        } break;

        case LWS_CALLBACK_CLIENT_RECEIVE: {
            transport::Message msg;

            ((char*)in)[len] = '\0';
            msg = (char*)in;
            logger->trace("RX: {}", (const char*)in);

            /**
             * we have two modes, if message callback was specified we pass
             * received messages to this callback, otherwise we push message to
             * rx queue where it will be handled by rx thread
             */
            if (!ws->message_)
                ws->rx_q_.push(msg);
            else
                ws->message_(msg);
        } break;

        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            transport::Message msg;
            std::vector<uint8_t> data;
            std::string msg_str;

            ws->tx_lock_.lock();
            if (!ws->tx_q_.empty()) {
                // get message from TX queue
                msg = ws->tx_q_.front();
                ws->tx_q_.pop();
                msg_str = msg.dump();

                logger->trace("TX: {} queue len {}", msg_str.c_str(),
                              ws->tx_q_.size());

                /* libwebsockets lws_write description says:
                 * For data being sent on a websocket
                 * connection (ie, not default http), this buffer MUST have
                 * LWS_PRE bytes valid BEFORE the pointer.
                 */
                data.resize(LWS_PRE + msg_str.size() +
                            LWS_SEND_BUFFER_POST_PADDING);
                memset(&data[0], 0, msg_str.size());
                memcpy(&data[LWS_PRE], msg_str.c_str(), msg_str.size());

                lws_write(wsi, &data[LWS_PRE], data.size() - LWS_PRE,
                          LWS_WRITE_TEXT);

                lws_callback_on_writable(wsi);
            }
            ws->tx_lock_.unlock();
        } break;

        case LWS_CALLBACK_CLIENT_CLOSED:
        case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
            /* If we are in async receive mode we have to drain rx queue before
             * notifying about connection close
             */
            if (!ws->message_)
                ws->drain_rx();

            if (ws->client_wsi_ != nullptr) {
                if (ws->disconnected_)
                    ws->run_on_rx_thread(ws->disconnected_);
                else
                    logger->debug("ws->disconnected_ is empty");
            }
            ws->client_wsi_ = NULL;
            break;
        case LWS_CALLBACK_EVENT_WAIT_CANCELLED: {
            logger->trace(
                "WEBSOCKET LWS_CALLBACK_EVENT_WAIT_CANCELLED ctx: {} wsi: {} "
                "this: {}",
                (void*)lws_get_context(wsi), (void*)wsi, (void*)ws);

            if (ws) {
                std::lock_guard<std::mutex> lock(ws->requests_q_lock_);

                /* Actual connection request event */
                if (!ws->requests_q_.empty()) {
                    struct lws* new_wsi = nullptr;

                    logger->trace("Connection request {}",
                                  ws->requests_q_.front());

                    if (!(new_wsi = ws->__connect(ws->requests_q_.front()))) {
                        logger->error("Connection to {} failed",
                                      ws->requests_q_.front());
                        if (ws->error_) {
                            ws->error_(nullptr, "Connection failed");
                        }
                        if (ws->disconnected_) {
                            ws->run_on_rx_thread(ws->disconnected_);
                            ws->close_connection_ = true;
                        }
                        ws->client_wsi_ = NULL;
                    } else {
                        logger->trace("LWS connection succeeded wsi: {}",
                                      (void*)new_wsi);
                    }

                    ws->requests_q_.pop();
                } else {
                    logger->trace("No connection requests queued");
                }

                /* Always return 0 here, because this wsi is events pipe */
                return 0;
            }
        } break;
        case LWS_CALLBACK_CLIENT_HTTP_DROP_PROTOCOL:
        case LWS_CALLBACK_WS_CLIENT_DROP_PROTOCOL:
        case LWS_CALLBACK_WSI_DESTROY:
            ws->close_connection_ = true;
            if (ws && ws->client_wsi_) {
                ws->client_wsi_ = NULL;
                /* If we are in async receive mode we have to drain rx queue
                 * before notifying about connection close
                 */
                if (!ws->message_)
                    ws->drain_rx();

                if (ws->disconnected_)
                    ws->run_on_rx_thread(ws->disconnected_);
                else if (ws->error_) {
                    ws->error_(nullptr, "Connection failed");
                    logger->debug("ws->disconnected_ is empty");
                } else {
                    logger->warn("No callbacks to notify connection closed");
                }
            }
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
            logger->info("Keep-alive from server received.");
            break;
        default:
            logger->trace("Unhandled reason {}", reason);
            break;
    }

    logger->debug("{}: reason {} ws {}, close_connection_ {} ", __func__,
                  reason, (void*)ws, ws ? ws->close_connection_ : false);

    /* returning -1 from this callback causes connection closure */
    if (ws && ws->close_connection_)
        return -1;

    return ret;
}

websocket::websocket(bool allow_invalid_ssl)
    : connected_(nullptr),
      disconnected_(nullptr),
      error_(nullptr),
      close_connection_(false),
      allow_invalid_ssl_(false) {}

websocket::websocket(std::function<void(transport::Data&)> handler,
                     bool allow_invalid_ssl)
    : lws_common(handler),
      connected_(nullptr),
      disconnected_(nullptr),
      error_(nullptr),
      close_connection_(false),
      allow_invalid_ssl_(allow_invalid_ssl) {}

bool websocket::start() {
    struct lws_context_creation_info info;
    int logs = LLL_USER | LLL_ERR | LLL_WARN;
    if (std::getenv("LWS_DEBUG"))
        logs |= LLL_INFO | LLL_NOTICE | LLL_DEBUG | LLL_CLIENT | LLL_LATENCY;

    lws_set_log_level(logs, NULL);

    assert(lws_context_ == nullptr);
    assert(client_wsi_ == nullptr);

    memset(&info, 0, sizeof info);
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
    info.protocols = _lws_protocols;
    info.user = this;
    info.timeout_secs = 20;
    info.connect_timeout_secs = 20;

    // ws ping/pong interval
    memset(&lws_retry_, 0, sizeof(lws_retry_));
    // how long to wait since last time we were told the connection is valid
    // before issuing our own PING to probe the connection.
    lws_retry_.secs_since_valid_ping = 10;
    // how long to wait since last time we were told the connection is valid
    // before hanging up the connection. Receiving a PONG tells us the
    // connection is valid. Here, we wait 60 - 10 == 50s for the PONG
    // to come before hanging up.
    lws_retry_.secs_since_valid_hangup = 60;

    info.retry_and_idle_policy = &lws_retry_;

#ifdef LWS_WITH_SOCKS5
    if (!proxy_.empty()) {
        char* p = (char*)proxy_.c_str();
        if (strstr(p, "://"))
            p = strstr(p, "://") + 3;

        logger->info("Websockets using proxy {}", p);

        if (p)
            info.socks_proxy_address = strdup(p);
    }
#endif

#if defined(LWS_WITH_TLS)
    /* Stop the ssl peers selecting a cipher which is too computationally
     * expensive */
#if defined(OPENSSL_CIPHERS_LIST)
    info.client_ssl_cipher_list = OPENSSL_CIPHERS_LIST;
#else
    info.client_ssl_cipher_list =
        "!RC4:!MD5:AES128-SHA:AES256-SHA:HIGH:!DSS:!aNULL";
#endif
#if defined(SSL_CERTS_BUNDLE_FILE)
    info.client_ssl_ca_filepath = SSL_CERTS_BUNDLE_FILE;
#endif
    logger->debug("TLS cipher list: {}", info.client_ssl_cipher_list);
#endif

    lws_context_ = lws_create_context(&info);
    if (!lws_context_) {
        logger->error("lws init failed");
        return false;
    }

    /* worker thread running loop_() which is libwebsockets main callback */
    return run();
}

bool websocket::stop() {
    transport::worker::term();

    if (lws_context_) {
        lws_cancel_service(lws_context_);
        lws_context_destroy(lws_context_);
    }

    lws_context_ = nullptr;

    return true;
}

bool websocket::disconnect() {
    /* As it's suggested in a libwebsockets documentation:
     * You can provoke a callback by calling lws_callback_on_writable on the
     * wsi, then notice in the callback you want to close it and just return -1.
     */
    close_connection_ = true;

    /* request write to actually close the connection */
    if (lws_context_ && client_wsi_)
        lws_callback_on_writable(client_wsi_);
    else
        return false;

    return true;
}

bool websocket::connect(std::string uri) {
    {
        std::lock_guard<std::mutex> lock(requests_q_lock_);

        logger->info("Enqueue websocket connect request to queue: {}", uri);

        requests_q_.push(uri);
    }

    if (lws_context_)
        lws_cancel_service(lws_context_);

    return true;
}

struct lws* websocket::__connect(std::string uri) {
    struct lws_client_connect_info i;
    const char *prot = nullptr, *address = nullptr, *path = nullptr;
    int port;
    char* p = (char*)uri.c_str();

    if (!lws_context_)
        return nullptr;

    if (lws_parse_uri(p, &prot, &address, &port, &path)) {
        logger->error("Unable to parse url {}", uri.c_str());
        return nullptr;
    }

    if (prot)
        ssl_ = !!strstr(prot, "wss");
    else
        ssl_ = false;

    if (address)
        host_ = address;

    port_ = port;
    path_ = "/";
    if (path)
        path_ += path;

    memset(&i, 0, sizeof i);
    i.context = lws_context_;
    i.port = port_;
    i.address = host_.c_str();
    i.path = path_.c_str();
    i.host = i.address;
    i.origin = i.address;
    if (ssl_) {
        i.ssl_connection = LCCSCF_USE_SSL;
#if defined(SSL_ALLOW_INSECURE_CERTS) && SSL_ALLOW_INSECURE_CERTS
        if (1)
#else
        if (allow_invalid_ssl_)
#endif
        {
            logger->warn("WSS client accepts insecure SSL cert!");
            i.ssl_connection |= LCCSCF_ALLOW_SELFSIGNED |
                                LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK |
                                LCCSCF_ALLOW_EXPIRED | LCCSCF_ALLOW_INSECURE;
        }
    }
    i.protocol = _lws_protocols[0].name;
    i.pwsi = &client_wsi_;
    i.userdata = this;

    close_connection_ = false;

    logger->info("Connecting to {}", host_.c_str());
    /* request async connect */
    return lws_client_connect_via_info(&i);
}

void* websocket::poll_() {
    ssize_t n = 0;
    utils::set_thread_name("websockets");
    while (n >= 0 && lws_context_ && running_)
        n = lws_service(lws_context_, 100);

    logger->info("polling thread stopped");

    return nullptr;
}

bool websocket::send(const transport::Data& message) {
    bool ret = true;

    /* put message to the tx queue */
    transport::worker::send(message);

    /* request write */
    if (lws_context_ && client_wsi_)
        lws_callback_on_writable(client_wsi_);
    else
        ret = false;

    return ret;
}

bool websocket::send(const transport::Message& message) {
    bool ret = true;

    /* put message to the tx queue */
    transport::worker::send(message);

    /* request write */
    if (lws_context_ && client_wsi_)
        lws_callback_on_writable(client_wsi_);
    else
        ret = false;

    return ret;
}
