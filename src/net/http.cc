#include "http.h"
#include <cassert>

#include <config.h>

using namespace vxg::cloud::transport::libwebsockets;

/* clang-format off */
static const struct lws_protocols _lws_protocols[] = {
    {
        "vxg-http-protocol",
        http::lws_callback_,
        sizeof (http::request),
        0,
    },
    {NULL, NULL, 0, 0}
};
/* clang-format on */

#define HTTP_SEND_BUFFER_SIZE (32 * 1024)
#define HTTP_RECV_BUFFER_SIZE (32 * 1024)

bool http::_do_request(request_ptr req) {
    struct lws_client_connect_info i;
    const char *prot = nullptr, *address = nullptr, *path = nullptr;
    int port;
    char p[4096];

    if (lws_snprintf(p, sizeof(p), "%s", (char*)req->url_.c_str()) !=
        req->url_.size()) {
        logger->error("HTTP URL is too long! Max len is {} url len {}",
                      sizeof(p), req->url_.size());
        return false;
    }

    if (lws_parse_uri(p, &prot, &address, &port, &path)) {
        logger->error("Unable to parse url {}", req->url_.c_str());
        return false;
    }

    if (prot)
        ssl_ = !!strstr(prot, "https");
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
    i.method = req->method_.c_str();

    if (req->auth_context_.type != request::AUTH_NONE) {
        logger->trace("HTTP Auth mode {}", req->auth_context_.type);
        i.auth_username = req->auth_context_.username.c_str();
        i.auth_password = req->auth_context_.password.c_str();
    }

    if (ssl_) {
        logger->trace("HTTP SSL enabled");
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

    if (req->method_ == "POST" && req->multipart_)
        i.ssl_connection |= LCCSCF_HTTP_MULTIPART_MIME;

    if (req->method_ != "GET")
        i.ssl_connection |= LCCSCF_HTTP_NO_CACHE_CONTROL;

    i.protocol = "";
    i.local_protocol_name = _lws_protocols[0].name;
    // i.pwsi = &req->client_wsi_;

    // we increase shared_ptr ref counter here, so we should decrease it when we
    // done with req
    i.userdata = static_cast<void*>(new request_ptr(req));
    req->close_connection_ = false;

    req->stats_start();

    /* request connect */
    auto wsi = lws_client_connect_via_info(&i);

    logger->trace("Connecting to {} wsi {}", i.address, (void*)wsi);

    return !!wsi;
}

static void _on_writable_delayed(struct lws_sorted_usec_list* sul) {
    struct http::request::delayed_on_writeable_payload* payload =
        lws_container_of(
            sul, struct http::request::delayed_on_writeable_payload, sul);

    lws_callback_on_writable(payload->wsi);
}

int http::lws_callback_(struct lws* wsi,
                        enum lws_callback_reasons reason,
                        void* user,
                        void* in,
                        size_t len) {
    request* _req = nullptr;
    request_ptr* _preq = nullptr;
    http* _http = (http*)lws_context_user(lws_get_context(wsi));
    vxg::logger::logger_ptr logger = _http->logger;
    char b[512];

    if (user) {
        _req = static_cast<std::shared_ptr<request>*>(user)->get();
        _preq = static_cast<request_ptr*>(user);
    }

    switch (reason) {
        case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
            return 0;
            break;
        case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP: {
            lws_get_peer_simple(wsi, b, sizeof(b));
            _req->status_ = lws_http_client_http_response(wsi);

            logger->trace("HTTP connection to {} established, status {} wsi {}",
                          b, _req->status_, (void*)wsi);

            if (_req->status_ == 204)
                lws_set_timeout(wsi, PENDING_TIMEOUT_AWAITING_SERVER_RESPONSE,
                                LWS_TO_KILL_ASYNC);
        } break;
        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
            unsigned char **p = (unsigned char**)in, *end = (*p) + len,
                          *start = *p;

            logger->trace("HTTP wsi {} auth {}", (void*)wsi,
                          _req->auth_context_.type);

            switch (_req->auth_context_.type) {
                case http::request::AUTH_ANY:
                case http::request::AUTH_BASIC: {
                    if (lws_http_basic_auth_gen(
                            _req->auth_context_.username.c_str(),
                            _req->auth_context_.password.c_str(), b,
                            sizeof(b))) {
                        logger->error("Unable to generate Basic auth header");
                        break;
                    }

                    if (lws_add_http_header_by_token(
                            wsi, WSI_TOKEN_HTTP_AUTHORIZATION,
                            (unsigned char*)b, (int)strlen(b), p, end)) {
                        logger->error("Unable to append Basic auth header");
                        return -1;
                    }
                } break;
                case http::request::AUTH_DIGEST: {
                } break;
                case http::request::AUTH_NONE: {
                } break;
                default: {
                } break;
            }

            if (!_req->request_body_.empty()) {
                if (!_req->multipart_) {
                    auto content_len =
                        std::to_string(_req->request_body_.size());

                    if (lws_add_http_header_by_token(
                            wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH,
                            (unsigned char*)content_len.c_str(),
                            (int)content_len.size(), p, end))
                        return -1;
                } else {
                    // Content-length formed as multipart prefix length +
                    // file length + postfix length
                    // We don't break file into chunks and send whole file as
                    // one chunk, the libcurl acts the same way.
                    // We calculate content length using lws api for multipart
                    // request and then reset internal wsi multipart state.
                    char _buf[LWS_PRE + HTTP_SEND_BUFFER_SIZE],
                        *_start = &_buf[LWS_PRE], *_p = _start,
                        *_end = &_buf[sizeof(_buf) - LWS_PRE - 1];
                    ssize_t multipart_prefix_postfix_len = 0;

                    if (lws_client_http_multipart(
                            wsi, _req->multipart_form_field_.c_str(),
                            _req->multipart_upload_filename_.c_str(),
                            _req->multipart_content_type_.c_str(), &_p, _end))
                        return -1;
                    if (lws_client_http_multipart(wsi, NULL, NULL, NULL, &_p,
                                                  _end))
                        return -1;
                    multipart_prefix_postfix_len = (_p - _start);
                    lws_client_http_multipart_reset(wsi);
                    // multipart prefix+postfix calculated, size = _p - _start;

                    if (multipart_prefix_postfix_len < 0) {
                        logger->error(
                            "Failed to calculate multipart prefix+postfix "
                            "length");
                        return -1;
                    }

                    auto content_len =
                        std::to_string(multipart_prefix_postfix_len +
                                       _req->request_body_.size());

                    if (lws_add_http_header_by_token(
                            wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH,
                            (unsigned char*)content_len.c_str(),
                            (int)content_len.size(), p, end))
                        return -1;
                }
            }

            lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_ACCEPT,
                                         (unsigned char*)"*/*", 3, p, end);

            if (!_req->headers_.empty()) {
                for (auto& kvp : _req->headers_) {
                    logger->debug("Append HTTP header {}:{} for url {}",
                                  kvp.first, kvp.second, _req->url_);
                    if (lws_add_http_header_by_name(
                            wsi, (const unsigned char*)kvp.first.c_str(),
                            (const unsigned char*)kvp.second.c_str(),
                            (int)kvp.second.size(), p, end)) {
                        return -1;
                    }
                }
            }

            if (!_req->request_body_.empty() &&
                !lws_http_is_redirected_to_get(wsi)) {
                logger->trace("{}: doing POST/PUT flow", __func__);

                _req->send_pos = 0;

                lws_client_http_body_pending(wsi, 1);
                lws_callback_on_writable(wsi);
            }
        } break;
        /* chunks of chunked content, with header removed */
        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ: {
            // logger->trace("LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ: read {}",
            //               (int)len);
            transport::Data _data;

            _req->response_body_.append((const char*)in, len);

            // Adjust timeout, 10 more seconds, lws closes connection
            // if long polling perfromed otherwise
            lws_set_timeout(wsi, PENDING_TIMEOUT_HTTP_CONTENT, 10);

            // ws->rx_lock_.lock();
            // ws->rx_q_.push(msg);
            // ws->rx_lock_.unlock();
        }
            return 0; /* don't passthru */

        /* uninterpreted http content */
        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP: {
            // logger->warn("LWS_CALLBACK_RECEIVE_CLIENT_HTTP");

            char buffer[HTTP_RECV_BUFFER_SIZE + LWS_PRE];
            char* px = buffer + LWS_PRE;
            int lenx = sizeof(buffer) - LWS_PRE;

            if (lws_http_client_read(wsi, &px, &lenx) < 0)
                return -1;

            lws_set_timeout(wsi, PENDING_TIMEOUT_AWAITING_SERVER_RESPONSE, 10);
        }
            return 0; /* don't passthru */

        case LWS_CALLBACK_COMPLETED_CLIENT_HTTP: {
            logger->trace("LWS_CALLBACK_COMPLETED_CLIENT_HTTP");

            _req->bad_ = _req->status_ != 200;

            if (lws_hdr_copy(wsi, b, sizeof(b), WSI_TOKEN_HTTP_CONTENT_LENGTH) >
                0)
                _req->response_headers_["content-length"] = b;

            if (lws_hdr_copy(wsi, b, sizeof(b), WSI_TOKEN_HTTP_CONTENT_TYPE) >
                0)
                _req->response_headers_["content-type"] = b;

            if (lws_hdr_copy(wsi, b, sizeof(b),
                             WSI_TOKEN_HTTP_WWW_AUTHENTICATE) > 0)
                _req->response_headers_["www-authenticate"] = b;

            // lws_cancel_service(lws_get_context(wsi)); /* abort poll wait */
            _req->close_connection_ = true;

        } break;

        case LWS_CALLBACK_CLOSED_CLIENT_HTTP: {
            logger->trace("LWS_CALLBACK_CLOSED_CLIENT_HTTP {}", _req->status_);

            _req->bad_ = _req->status_ != 200;

            _req->stats_finalize();

            if (_req->response_)
                _req->response_(_req->response_body_, _req->status_);
            else
                logger->trace("Response cb is empty!");

            // lws_cancel_service(lws_get_context(wsi)); /* abort poll wait */
            _req->close_connection_ = true;
        } break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            logger->error("CLIENT_CONNECTION_ERROR: {}",
                          in ? (char*)in : "(null)");

            _req->stats_finalize();

            if (_req->response_)
                _req->response_(_req->response_body_, _req->status_);

            _req->close_connection_ = true;
            _req->status_ = -1;
            break;

        case LWS_CALLBACK_CLIENT_ESTABLISHED: {
            logger->trace("{}: established", __func__);
        } break;

        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            logger->trace("LWS_CALLBACK_CLIENT_WRITEABLE");
        } break;

        case LWS_CALLBACK_CLIENT_CLOSED: {
            logger->trace("LWS_CALLBACK_CLIENT_CLOSED");
        } break;
        case LWS_CALLBACK_EVENT_WAIT_CANCELLED: {
            logger->trace("HTTP LWS_CALLBACK_EVENT_WAIT_CANCELLED ctx: {}",
                          (void*)_http);
            if (_http) {
                std::lock_guard<std::mutex> lock(_http->requests_q_lock_);

                if ((!_http->requests_q_.empty())) {
                    /* Actual connection request event */
                    while (!_http->requests_q_.empty()) {
                        logger->trace(
                            "Connection request dequeued {} waiting "
                            "connections {}",
                            (void*)_http->requests_q_.front().get(),
                            _http->requests_q_.size());

                        if (!_http->_do_request(_http->requests_q_.front())) {
                            logger->error("Connection to {} failed",
                                          _http->requests_q_.front()->url_);

                            _http->requests_q_.front()->bad_ = 1;
                            _http->requests_q_.front()->status_ = -1;
                            _http->requests_q_.front()->close_connection_ =
                                true;

                            _http->requests_q_.front()->notify_finish();
                        }

                        _http->requests_q_.pop();
                    }
                    /* Always return 0 here, because this wsi is events pipe */
                    return 0;
                }
            }
        } break;
        case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE: {
            lws_write_protocol write_flag = LWS_WRITE_HTTP;
            char buf[LWS_PRE + HTTP_SEND_BUFFER_SIZE],
                *start = &buf[LWS_PRE], *p = start,
                *end = &buf[sizeof(buf) - LWS_PRE - 1];
            size_t processing_len =
                std::min(_req->request_body_.size() - _req->send_pos,
                         (size_t)(end - start));

            if (lws_http_is_redirected_to_get(wsi))
                break;

            _req->stats_update();
            if (_req->max_upload_speed > 0 &&
                _req->upload_speed > _req->max_upload_speed) {
                logger->debug("Limiting upload speed {} to requested {}",
                              _req->upload_speed, _req->max_upload_speed);

                _req->delayed_on_write_payload.wsi = wsi;

                memset(&_req->delayed_on_write_payload.sul, 0,
                       sizeof(lws_sorted_usec_list_t));
                // Ask to call us again later
                lws_sul_schedule(lws_get_context(wsi), 0,
                                 &_req->delayed_on_write_payload.sul,
                                 _on_writable_delayed, 100 * LWS_US_PER_MS);
                return 0;
            }

            // Multipart POST
            if (_req->multipart_) {
                if (_req->send_pos == 0) {
                    if (lws_client_http_multipart(
                            wsi, _req->multipart_form_field_.c_str(),
                            _req->multipart_upload_filename_.c_str(),
                            _req->multipart_content_type_.c_str(), &p, end))
                        return -1;
                }

                // update len after adding multipart header
                processing_len =
                    std::min(_req->request_body_.size() - _req->send_pos,
                             (size_t)(end - p));
            }

            // put chunk of request body to send buffer
            memcpy(p, &_req->request_body_[_req->send_pos], processing_len);
            p += processing_len;
            _req->send_pos += processing_len;

            logger->trace(
                "LWS_CALLBACK_CLIENT_HTTP_WRITEABLE {} wsi {} sent {}",
                processing_len, (void*)wsi, _req->send_pos);

            if (_req->request_body_.size() > _req->send_pos) {
                // Set timeout for the next LWS_CALLBACK_CLIENT_HTTP_WRITEABLE
                // If the connection speed is slow and the kernel tcp buffer is
                // not draining the LWS_CALLBACK_CLIENT_HTTP_WRITEABLE will not
                // be triggered and lws will close the connection
                lws_set_timeout(wsi, PENDING_TIMEOUT_HTTP_CONTENT, 30);
                write_flag = LWS_WRITE_HTTP;
            } else {
                if (!_req->multipart_) {
                    logger->trace("HTTP final write");
                } else {
                    // Multipart POST postfix
                    if (lws_client_http_multipart(wsi, NULL, NULL, NULL, &p,
                                                  end))
                        return -1;

                    logger->trace("HTTP Multipart POST final write");
                }

                lws_client_http_body_pending(wsi, 0);
                write_flag = LWS_WRITE_HTTP_FINAL;
            }

            // write data from send buffer
            if (lws_write(wsi, (uint8_t*)start, lws_ptr_diff(p, start),
                          write_flag) != lws_ptr_diff(p, start)) {
                logger->error("HTTP write failed");
                return -1;
            }

            // request next write callback if we have to send data left and
            // or set no more data otherwise
            if (write_flag != LWS_WRITE_HTTP_FINAL) {
                auto ret = lws_callback_on_writable(wsi);
                logger->trace("lws_callback_on_writable() -> {}", ret);
                if (ret < 1)
                    logger->error("lws_callback_on_writable() failed!");
            }

            return 0;
        } break;
        case LWS_CALLBACK_CLIENT_HTTP_DROP_PROTOCOL: {
            // logger->trace("LWS_CALLBACK_CLIENT_HTTP_DROP_PROTOCOL: {}",
            //               (void*)in);
        } break;
        case LWS_CALLBACK_WSI_DESTROY: {
            // Deref request
            logger->trace("Request {} unrefed", _preq->use_count());
            delete _preq;
            return 1;
        }
        case LWS_CALLBACK_OPENSSL_PERFORM_SERVER_CERT_VERIFICATION:
            logger->trace("CERT_VERIFICATION");
            return 0;
            break;
        default:
            logger->trace("Unhandled reason {} wsi {}", reason, (void*)wsi);
            break;
    }

    /* returning -1 from this callback causes connection close */
    if (_req && _req->close_connection_) {
        logger->trace("Notify finish from wsi {} reason {}", (void*)wsi,
                      reason);

        _req->notify_finish();
        // kill client's wsi
        return -1;
    }

    return lws_callback_http_dummy(wsi, reason, user, in, len);
}

http::http(bool allow_invalid_ssl)
    : lws_common(nullptr), allow_invalid_ssl_ {allow_invalid_ssl} {}

bool http::start() {
    struct lws_context_creation_info info;
    int logs = LLL_USER | LLL_ERR;
    if (std::getenv("LWS_DEBUG"))
        logs |= LLL_INFO | LLL_NOTICE | LLL_DEBUG | LLL_CLIENT | LLL_LATENCY;
    lws_set_log_level(logs, NULL);

    if (running_)
        return true;

    assert(lws_context_ == nullptr);

    memset(&info, 0, sizeof(info));
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
    info.protocols = _lws_protocols;
    info.user = this;
    info.timeout_secs = 30;
    info.connect_timeout_secs = 10;

    // ws ping/pong interval
    memset(&lws_retry_, 0, sizeof(lws_retry_));
    // how long to wait since last time we were told the connection is valid
    // before issuing our own PING to probe the connection.
    lws_retry_.secs_since_valid_ping = 10;
    // how long to wait since last time we were told the connection is valid
    // before hanging up the connection. Receiving a PONG tells us the
    // connection is valid. Here, we wait 30 - 10 == 20s for the PONG
    // to come before hanging up.
    lws_retry_.secs_since_valid_hangup = 30;

    // Use only 2 elements of default retry table, 1s and 3s
    lws_retry_.conceal_count = 2;
    lws_retry_.retry_ms_table_count = 2;

    info.retry_and_idle_policy = &lws_retry_;

#ifdef LWS_WITH_SOCKS5
    if (!proxy_.empty()) {
        char* p = (char*)proxy_.c_str();
        if (strstr(p, "://"))
            p = strstr(p, "://") + 3;

        logger->info("HTTP using proxy {}", p);

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
    logger->debug("TLS cipher list: {}", info.client_ssl_cipher_list);
#endif

    // info.max_http_header_pool = 4;

    lws_context_ = lws_create_context(&info);
    if (!lws_context_) {
        logger->error("lws init failed");
        return false;
    }

    /* worker thread running loop_() which is libwebsockets main callback */
    return run();
}

bool http::stop() {
    /* blocked stopping polling thread */
    worker::term();

    if (lws_context_)
        lws_context_destroy(lws_context_);

    lws_context_ = nullptr;

    return true;
}

bool http::is_secure() {
#ifdef LWS_WITH_TLS
    return true;
#else
    return false;
#endif
}

/**
 * libwebsockets is not thread safe, all lws api should be called
 * in the single thread, the only thread-safe function is lws_cancel_service()
 *
 * According to suggestions from the lws docs this function places connection
 * info into the shared memory(requests queue) protected by mutex and
 * triggers lws callback event LWS_CALLBACK_EVENT_WAIT_CANCELLED by the
 * lws_cancel_service() call.
 */
bool http::make(request_ptr req) {
    {
        std::lock_guard<std::mutex> lock(requests_q_lock_);

        logger->debug("HTTP {} request scheduled, url: {}", req->method_,
                      req->url_);

        requests_q_.push(req);
    }

    if (lws_context_)
        lws_cancel_service(lws_context_);

    return true;
}

bool http::get(request_ptr req) {
    req->method_ = "GET";

    return http::make_blocked(req);
}

bool http::post(request_ptr req) {
    req->method_ = "POST";

    return http::make_blocked(req);
}

bool http::make_blocked(request_ptr req) {
    std::unique_lock<std::mutex> lock(req->lock_);

    req->response_headers_.clear();
    req->response_body_.clear();

    http::make(req);

    if (!req->cond_.wait_for(lock, req->timeout_s_,
                             [&req] { return req->close_connection_; })) {
        // timeout
        logger->info(
            "HTTP request didn't finish in {} seconds, closing connection.",
            (int)req->timeout_s_.count());
        req->close_connection_ = true;
        return false;
    } else {
        logger->trace("http::make signal received");
    }

    return true;
}

void* http::poll_() {
    ssize_t n = 0;
    cloud::utils::set_thread_name("http");
    while (n >= 0 && lws_context_ && running_)
        n = lws_service(lws_context_, 100);

    logger->info("http transport lws service thread stopped");

    return nullptr;
}
