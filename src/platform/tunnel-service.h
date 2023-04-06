#ifndef __TUNNEL_SERVICE_H
#define __TUNNEL_SERVICE_H

#include <libssh2.h>

#ifdef WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>

#include <agent-proto/proto.h>
#include <net/http.h>
#include <net/websockets.h>
#include <utils/base64.h>
#include <utils/logging.h>
#include <utils/utils.h>

#ifndef INADDR_NONE
#define INADDR_NONE (in_addr_t) - 1
#endif

using namespace vxg::cloud;

namespace openssl {
class rsa_keygen {
    vxg::logger::logger_ptr logger =
        vxg::logger::instance("openssl-rsa-keygen");
    rsa_keygen() {}

    int _ssh_encode_buffer(unsigned char* enc, int len, unsigned char* buf) {
        int adj_len = len, index;
        if (*buf & 0x80) {
            adj_len++;
            enc[4] = 0;
            index = 5;
        } else {
            index = 4;
        }
        enc[0] = (unsigned char)(adj_len >> 24);
        enc[1] = (unsigned char)(adj_len >> 16);
        enc[2] = (unsigned char)(adj_len >> 8);
        enc[3] = (unsigned char)(adj_len);
        memcpy(&enc[index], buf, len);
        return index + len;
    }

    bool _generate_rsa_keypair(int numBits,
                               std::string& privKey,
                               std::string& pubKey) {
        RSA* rsa = RSA_new();
        BIGNUM* bn = BN_new();
        BIO* privKeyBuff = BIO_new(BIO_s_mem());
        BIO* pubKeyBuff = BIO_new(BIO_s_mem());
        char* privKeyData;
        char* pubKeyData;

        BN_set_word(bn, RSA_F4);
        RSA_generate_key_ex(rsa, numBits, bn, nullptr);

        PEM_write_bio_RSAPrivateKey(privKeyBuff, rsa, 0, 0, 0, 0, 0);
        PEM_write_bio_RSA_PUBKEY(pubKeyBuff, rsa);

        auto privKeySize = BIO_get_mem_data(privKeyBuff, &privKeyData);
        auto pubKeySize = BIO_get_mem_data(pubKeyBuff, &pubKeyData);

        privKey = std::string(privKeyData, privKeySize);
        pubKey = std::string(pubKeyData, pubKeySize);

        privKey += '\n';
        privKey += pubKey;

        BIO_free_all(privKeyBuff);
        BIO_free_all(pubKeyBuff);
        BN_free(bn);
        RSA_free(rsa);

        return true;
    }

    // return copy of the public key in ssh format
    std::string _public_key_pem_to_ssh(std::string public_key_pem) {
        char result_key[4096] = {0};
        RSA* rsa = nullptr;
        BIO *bufio, *bio, *b64;
        int n_len = 0, e_len = 0;
        int enc_len = 0;
        int index = 0;
        unsigned char *n_bytes = NULL, *e_bytes = NULL;
        unsigned char* enc = NULL;
        unsigned char ssh_header[] = {0x00, 0x00, 0x00, 0x07, 0x73, 0x73,
                                      0x68, 0x2D, 0x72, 0x73, 0x61};
        const BIGNUM *n, *e;
        std::string result;

        bufio = BIO_new_mem_buf(public_key_pem.c_str(), public_key_pem.size());
        rsa = PEM_read_bio_RSA_PUBKEY(bufio, &rsa, nullptr, nullptr);

        if (rsa == NULL) {
            /* an error occurred */
            ERR_load_ERR_strings();
            ERR_load_crypto_strings();
            unsigned long ulErr = ERR_get_error();
            char err_msg[1024] = {0};
            ERR_error_string(ulErr, err_msg);
            std::string errcode(err_msg);

            logger->error("Failed to load public RSA key, error: {}", errcode);
            return "";
        }

        // reading the modulus and public exponent
#if !defined(OPENSSL_VERSION_LESS_THAN_1_1_0)
        RSA_get0_key(rsa, &n, &e, nullptr);
#else
        n = rsa->n;
        e = rsa->e;
#endif
        n_len = RSA_size(rsa);
        n_bytes = (unsigned char*)malloc(n_len);
        BN_bn2bin(n, n_bytes);
        e_len = BN_num_bytes(e);
        e_bytes = (unsigned char*)malloc(e_len);
        BN_bn2bin(e, e_bytes);

        enc_len = 11 + 4 + e_len + 4 + n_len;
        // correct depending on the MSB of e and N
        if (e_bytes[0] & 0x80)
            enc_len++;
        if (n_bytes[0] & 0x80)
            enc_len++;

        enc = (unsigned char*)malloc(enc_len);
        memcpy(enc, ssh_header, 11);

        index = _ssh_encode_buffer(&enc[11], e_len, e_bytes);
        index = _ssh_encode_buffer(&enc[11 + index], n_len, n_bytes);

        b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        bio = BIO_new(BIO_s_mem());
        BIO_printf(bio, "ssh-rsa ");
        bio = BIO_push(b64, bio);
        BIO_write(bio, enc, enc_len);
        BIO_flush(bio);
        bio = BIO_pop(b64);
        BIO_printf(bio, " %s_%s\n", "VXG",
                   utils::time::now_ISO8601_UTC_packed().c_str());
        BIO_flush(bio);
        BIO_read(bio, result_key, sizeof(result_key));

        result = std::string(result_key);

        BIO_free_all(bio);
        BIO_free(b64);
        BIO_free_all(bufio);

        if (rsa)
            RSA_free(rsa);
        if (n_bytes)
            free(n_bytes);
        if (e_bytes)
            free(e_bytes);
        if (enc)
            free(enc);

        return result;
    }

public:
    ~rsa_keygen() {}

    std::pair<std::string, std::string> generate_keypair() {
        std::string priv, pub;

        _generate_rsa_keypair(1024, priv, pub);

        return std::pair<std::string, std::string>(priv, pub);
    }

    std::string public_pem2ssh(std::string public_key_pem) {
        return _public_key_pem_to_ssh(public_key_pem);
    }

    static rsa_keygen& instance() {
        static rsa_keygen r;
        return r;
    }
};
}  // namespace openssl
namespace libssh2 {

enum tunnel_status {
    TS_IDLE,
    TS_OK,
    TS_SSH_CONN_FAILED,
    TS_SSH_TUNN_FAILED,
    TS_CLOSED,
    TS_EXPIRED,

    TS_INVALID = -1,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( tunnel_status, {
    {TS_IDLE, "Tunnel is not started yet"},
    {TS_OK, "Tunnel Opened"},
    {TS_SSH_CONN_FAILED, "Failed to connect to ssh server."},
    {TS_SSH_TUNN_FAILED, "Tunnel creation failed."},
    {TS_CLOSED, "Tunnel closed by server."},
    {TS_EXPIRED, "Tunnel timed out."},

    {TS_INVALID, nullptr},
})
// clang-format on
class reverse_tunnel : public std::enable_shared_from_this<reverse_tunnel> {
public:
    typedef std::shared_ptr<reverse_tunnel> ptr;
    struct params {
        std::string ssh_server;
        uint16_t ssh_server_port {22};
        std::string ssh_server_username;
        std::string ssh_server_private_key;
        // RSA keypair in PEM format
        std::string ssh_private_key;
        std::string ssh_public_key;

        std::string local_dest_ip {"127.0.0.1"};
        uint16_t local_dest_port;
        std::string remote_listen_host {"0.0.0.0"};
        uint16_t remote_listen_port;

        int timeout;
    };

private:
    static constexpr const char* TAG = "libssh2-tunnel";
    static std::once_flag libssh_init_flag_;
    params params_;
    std::atomic_bool running_ {false};
    std::thread thread_acceptor_;
    int ssh_session_sock_ {-1};
    LIBSSH2_SESSION* ssh_session_ {nullptr};
    LIBSSH2_LISTENER* ssh_listener_ {nullptr};

    struct ssh_channel {
        typedef std::shared_ptr<ssh_channel> ptr;

        LIBSSH2_CHANNEL* ssh_channel;
        std::vector<uint8_t> rx;
        std::vector<uint8_t> tx;
    };

    std::map<int, ssh_channel::ptr> ssh_channels_;
    std::string private_key_;
    std::string public_key_;
    tunnel_status status_ {TS_IDLE};

    bool _create_ssh_session() {
        int rc = -1;
        const char* pub_key = public_key_.c_str();
        const char* priv_key = private_key_.c_str();
        size_t pub_key_len = public_key_.size();
        size_t priv_key_len = private_key_.size();

        ssh_session_sock_ =
            _connect(params_.ssh_server, params_.ssh_server_port);
        if (ssh_session_sock_ == -1) {
            vxg::logger::instance(TAG)->error(
                "Failed to connect to ssh server {}:{}", params_.ssh_server,
                params_.ssh_server_port);
            goto end;
        }

        /* Create a session instance */
        if (!(ssh_session_ = libssh2_session_init())) {
            vxg::logger::instance(TAG)->error(
                "Could not initialize SSH session!");
            goto end;
        }
        // libssh2_trace(ssh_session_, LIBSSH2_TRACE_CONN | LIBSSH2_TRACE_ERROR
        // |
        //                                 LIBSSH2_TRACE_SOCKET);

        /* ... start it up. This will trade welcome banners, exchange keys,
         * and setup crypto, compression, and MAC layers
         */
        if ((rc = libssh2_session_handshake(ssh_session_, ssh_session_sock_))) {
            vxg::logger::instance(TAG)->error(
                "SSH initial handshake failed: {}", rc);
            goto end;
        }

        vxg::logger::instance(TAG)->info(
            "SSH session {} with {}:{} created", (void*)ssh_session_,
            params_.ssh_server, params_.ssh_server_port);

        if (!params_.ssh_private_key.empty()) {
            priv_key = params_.ssh_private_key.c_str();
            priv_key_len = params_.ssh_private_key.size();
        }

        pub_key = nullptr;
        pub_key_len = 0;

        if (libssh2_userauth_publickey_frommemory(
                ssh_session_, params_.ssh_server_username.c_str(),
                params_.ssh_server_username.size(), pub_key, pub_key_len,
                priv_key, priv_key_len, nullptr)) {
            vxg::logger::instance(TAG)->error(
                "SSH authentication by public key failed!");
            goto end;
        }
        vxg::logger::instance(TAG)->info(
            "Authentication by public key succeeded.");

        return true;
    end:
        if (ssh_session_sock_ != -1)
            close(ssh_session_sock_);

        if (ssh_session_) {
            libssh2_session_disconnect(ssh_session_,
                                       "Client disconnecting normally");
            libssh2_session_free(ssh_session_);
            ssh_session_ = nullptr;
        }

        return false;
    }

    bool _channel_forward_remote_listen(int want_port, int& bound_port) {
        int real_port = -1;
        vxg::logger::instance(TAG)->info(
            "Asking server to listen on remote {}:{}",
            params_.remote_listen_host, params_.remote_listen_port);

        ssh_listener_ = libssh2_channel_forward_listen_ex(
            ssh_session_, params_.remote_listen_host.c_str(), want_port,
            &real_port, 200);
        bound_port = real_port;

        if (!ssh_listener_) {
            vxg::logger::instance(TAG)->error(
                "Could not start the tcpip-forward listener!"
                "(Note that this can be a problem at the server!"
                " Please review the server logs.)");
            return false;
        }

        libssh2_session_set_blocking(ssh_session_, 0);

        return true;
    }

    static int _connect(std::string addr, const uint16_t port) {
        int forwardsock = -1;
        struct sockaddr_in sin;
        socklen_t sinlen = sizeof(sin);

        forwardsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (forwardsock == -1) {
            vxg::logger::instance(TAG)->error("Failed to create socket");
            goto end;
        }

        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        sin.sin_addr.s_addr = inet_addr(addr.c_str());

        if (INADDR_NONE == sin.sin_addr.s_addr) {
            vxg::logger::instance(TAG)->error("inet_addr");
            goto end;
        }

        if (0 != connect(forwardsock, (struct sockaddr*)&sin, sinlen)) {
            vxg::logger::instance(TAG)->error("connect");
            goto end;
        }

        return forwardsock;
    end:
        if (forwardsock != -1)
            close(forwardsock);
        return -1;
    }

    static int _wait_socket(int socket_fd, LIBSSH2_SESSION* session) {
        struct timeval timeout;
        int rc;
        fd_set fd;
        fd_set* writefd = NULL;
        fd_set* readfd = NULL;
        int dir;

        timeout.tv_sec = 0;
        timeout.tv_usec = 1000;

        FD_ZERO(&fd);

        FD_SET(socket_fd, &fd);

        /* now make sure we wait in the correct direction */
        if (session) {
            dir = libssh2_session_block_directions(session);
            if (dir & LIBSSH2_SESSION_BLOCK_INBOUND)
                readfd = &fd;

            if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
                writefd = &fd;
        } else {
            readfd = &fd;
            writefd = &fd;
        }

        rc = select(socket_fd + 1, readfd, writefd, NULL, &timeout);

        return rc;
    }

    static bool _set_nonblocking(int fd, bool blocking) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1)
            return false;
        flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
        return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
    }

    static void _open_channel(reverse_tunnel::ptr tunnel) {
        auto channel = libssh2_channel_forward_accept(tunnel->ssh_listener_);

        if (channel) {
            int local_sock = -1;
            vxg::logger::instance(TAG)->info(
                "Accepted remote connection. Connecting to local server "
                "{}:{}",
                tunnel->params_.local_dest_ip, tunnel->params_.local_dest_port);

            local_sock = _connect(tunnel->params_.local_dest_ip.c_str(),
                                  tunnel->params_.local_dest_port);

            if (local_sock > 0) {
                ssh_channel::ptr _channel = std::make_shared<ssh_channel>();
                _channel->ssh_channel = channel;

                tunnel->ssh_channels_[local_sock] = _channel;

                _set_nonblocking(local_sock, true);

                vxg::logger::instance(TAG)->info(
                    "Local connection {} established.", local_sock);
            } else {
                vxg::logger::instance(TAG)->error(
                    "Local connection NOT established.");
                libssh2_channel_free(channel);
            }
        }
    }

    static void _close_channel(reverse_tunnel* tunnel, int fd) {
        vxg::logger::instance(TAG)->info(
            "Closing local connection {} and ssh channel {}", fd,
            (void*)tunnel->ssh_channels_[fd]->ssh_channel);
        close(fd);
        libssh2_channel_free(tunnel->ssh_channels_[fd]->ssh_channel);
    }

    reverse_tunnel(params tun_params) {
        params_ = tun_params;

        if (params_.ssh_private_key.empty()) {
            if (params_.ssh_private_key.empty()) {
                auto keypair =
                    openssl::rsa_keygen::instance().generate_keypair();
                private_key_ = keypair.first;
                public_key_ = keypair.second;
            } else {
                private_key_ = params_.ssh_private_key;
                public_key_ = params_.ssh_public_key;
            }
        }
    }

public:
    ~reverse_tunnel() { stop(); }

    static reverse_tunnel::ptr create(params tun_params) {
        try {
            std::call_once(reverse_tunnel::libssh_init_flag_, []() {
                if (libssh2_init(0))
                    throw std::exception();
            });
        } catch (...) {
            vxg::logger::instance(TAG)->error("Failed to init libssh2");
            return nullptr;
        }

        struct make_shared_enabler : public reverse_tunnel {
            make_shared_enabler(params p) : reverse_tunnel(p) {}
        };
        return std::make_shared<make_shared_enabler>(tun_params);
    }

    static void channels_loop(reverse_tunnel::ptr tunnel) {
        int ssh_rc, rc;
        fd_set rd_fds, wr_fds;
        char buf[1024 * 64];
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 1000;
        int max_fd = -1;

        /* Fill fdsets with local connections descriptors */
        FD_ZERO(&rd_fds);
        FD_ZERO(&wr_fds);
        for (auto& kv : tunnel->ssh_channels_) {
            FD_SET(kv.first, &rd_fds);
            FD_SET(kv.first, &wr_fds);
            max_fd = std::max(max_fd, kv.first);
        }

        /* Wait for activity on local connections */
        rc = select(max_fd + 1, &rd_fds, &wr_fds, nullptr, &tv);
        if (rc > 0) {
            /* Loop over local connections and read/write where it's possible */
            for (auto kv = tunnel->ssh_channels_.cbegin();
                 kv != tunnel->ssh_channels_.cend();) {
                int i = 0;
                bool can_read = FD_ISSET(kv->first, &rd_fds);
                bool can_write = FD_ISSET(kv->first, &wr_fds);

                /* Read from the local connection and put to the local rx buf */
                if (can_read && kv->second->rx.size() < (1024 * 1024)) {
                    i = recv(kv->first, buf, sizeof(buf), 0);
                    // vxg::logger::instance(TAG)->info("Read {} bytes from
                    // local conn {}", i,
                    //              kv->first);
                    if (i > 0)
                        kv->second->rx.insert(kv->second->rx.end(), buf,
                                              buf + i);
                    else if (i <= 0) {
                        /* recv() returns 0 if peer closed connection */
                        _close_channel(tunnel.get(), kv->first);
                        tunnel->ssh_channels_.erase(kv++);
                        continue;
                    }
                }

                /* Write to the local connection */
                if (can_write && !kv->second->tx.empty()) {
                    i = send(kv->first, kv->second->tx.data(),
                             kv->second->tx.size(), 0);
                    // vxg::logger::instance(TAG)->info("Wrote {} bytes to local
                    // conn {}", i,
                    //              kv->first);
                    if (i > 0)
                        kv->second->tx.erase(kv->second->tx.begin(),
                                             kv->second->tx.begin() + i);
                    else if (i < 0) {
                        _close_channel(tunnel.get(), kv->first);
                        tunnel->ssh_channels_.erase(kv++);
                        continue;
                    }
                }

                /* Wait for possible activity on ssh connection */
                ssh_rc = _wait_socket(tunnel->ssh_session_sock_,
                                      tunnel->ssh_session_);
                if (ssh_rc != LIBSSH2_ERROR_EAGAIN) {
                    /* Write data received from the local connection to the ssh
                     * tunnel's connection corresponding to the ssh channel.
                     */
                    while (!kv->second->rx.empty()) {
                        i = libssh2_channel_write(
                            kv->second->ssh_channel,
                            (const char*)kv->second->rx.data(),
                            kv->second->rx.size());

                        if (i == LIBSSH2_ERROR_EAGAIN) {
                            _wait_socket(tunnel->ssh_session_sock_,
                                         tunnel->ssh_session_);
                            continue;
                        } else if (i > 0) {
                            kv->second->rx.erase(kv->second->rx.begin(),
                                                 kv->second->rx.begin() + i);
                            break;
                        }
                    }
                    if (i < 0 && i != LIBSSH2_ERROR_EAGAIN) {
                        _close_channel(tunnel.get(), kv->first);
                        tunnel->ssh_channels_.erase(kv++);
                        continue;
                    }

                    /* Read from ssh channel and put data to the local buffer */
                    i = libssh2_channel_read(kv->second->ssh_channel, buf,
                                             sizeof(buf));
                    if (i < 0 && LIBSSH2_ERROR_EAGAIN != i) {
                        _close_channel(tunnel.get(), kv->first);
                        tunnel->ssh_channels_.erase(kv++);
                        continue;
                    } else if (i > 0) {
                        kv->second->tx.insert(kv->second->tx.end(), buf,
                                              buf + i);
                    }

                    /* Close channel if tunnel's client closed connection */
                    if (libssh2_channel_eof(kv->second->ssh_channel)) {
                        _close_channel(tunnel.get(), kv->first);
                        tunnel->ssh_channels_.erase(kv++);
                        continue;
                    }
                }

                kv++;
            }
        }
    }

    static void acceptor_routine(reverse_tunnel::ptr tunnel) {
        vxg::logger::instance(TAG)->info("Waiting for remote connections");

        while (tunnel->running_) {
            int rc;

            // Wait for the activity on the main ssh socket.
            if ((rc = _wait_socket(tunnel->ssh_session_sock_,
                                   tunnel->ssh_session_)) < 0) {
                vxg::logger::instance(TAG)->error("wait socket");
                tunnel->status_ = TS_CLOSED;
                break;
            }

            // Try to accept new tunnel's client, nothing bad if there is none
            if (rc != LIBSSH2_ERROR_EAGAIN)
                _open_channel(tunnel);

            // Loop over all channels
            channels_loop(tunnel);
        }

        vxg::logger::instance(TAG)->info("{}:{} forwarding thread stopped",
                                         tunnel->params_.local_dest_ip,
                                         tunnel->params_.local_dest_port);
    }

    bool init_forward_session() { return _create_ssh_session(); }

    bool bind_remote_forward_port(int& dyn_port) {
        int bound_port = -1;
        if (_channel_forward_remote_listen(params_.remote_listen_port,
                                           bound_port)) {
            dyn_port = bound_port;
            return true;
        }

        return false;
    }

    bool start() {
        int dyn_port = -1;

        if (!init_forward_session()) {
            status_ = TS_SSH_CONN_FAILED;
            goto end;
        }

        if (!bind_remote_forward_port(dyn_port)) {
            status_ = TS_SSH_TUNN_FAILED;
            goto end;
        }

        vxg::logger::instance(TAG)->info("Bound port {}", dyn_port);

        status_ = TS_OK;
        running_ = true;
        thread_acceptor_ =
            std::thread(reverse_tunnel::acceptor_routine, shared_from_this());
        return true;
    end:
        return false;
    }

    bool running() { return running_; }

    void stop() {
        running_ = false;
        if (thread_acceptor_.joinable() &&
            thread_acceptor_.get_id() != std::this_thread::get_id()) {
            vxg::logger::instance(TAG)->info(
                "Waiting for forwarding thread stop");
            thread_acceptor_.join();
            vxg::logger::instance(TAG)->info("Forwarding thread stopped");
        } else if (thread_acceptor_.get_id() != std::thread::id()) {
            thread_acceptor_.detach();
            vxg::logger::instance(TAG)->info("Forwarding thread detached");
        }

        if (ssh_session_)
            libssh2_session_set_blocking(ssh_session_, 1);

        if (ssh_listener_) {
            libssh2_channel_forward_cancel(ssh_listener_);
            ssh_listener_ = nullptr;
        }

        for (auto c = ssh_channels_.cbegin(); c != ssh_channels_.cend();) {
            _close_channel(this, c->first);
            ssh_channels_.erase(c++);
        }

        if (ssh_session_) {
            libssh2_session_disconnect(ssh_session_,
                                       "Client disconnecting normally");
            libssh2_session_free(ssh_session_);
            ssh_session_ = nullptr;
        }

        if (ssh_session_sock_ > 0)
            close(ssh_session_sock_);
    }

    void set_params(params p) { params_ = p; }

    tunnel_status get_status() { return status_; }
};

std::once_flag reverse_tunnel::libssh_init_flag_;
}  // namespace libssh2

enum port_forward_cmd {
    PF_GET_KEY,
    PF_CREATE_TUNNEL,
    PF_DELETE_TUNNEL,

    PF_INVALID = -1,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( port_forward_cmd, {
    {PF_GET_KEY, "get_key"},
    {PF_CREATE_TUNNEL, "create_tunnel"},
    {PF_DELETE_TUNNEL, "delete_tunnel"},

    {PF_INVALID, nullptr},
})
// clang-format on

enum port_forward_status {
    PFS_OK,
    PFS_ERROR,
    PFS_NOT_ALLOWED,

    PFS_INVALID = -1,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM( port_forward_status, {
    {PFS_OK, "Ok"},
    {PFS_ERROR, "Error"},
    {PFS_NOT_ALLOWED, "Not allowed"},

    {PFS_INVALID, nullptr},
})
// clang-format on

struct port_forward_params {
    std::string cam_ip;
    int cam_port;
    int port_out;
    std::string user;
    std::string host;
    int timeout;

    JSON_DEFINE_TYPE_INTRUSIVE(port_forward_params,
                               cam_ip,
                               cam_port,
                               port_out,
                               user,
                               host,
                               timeout);
};

struct port_forward_message {
    port_forward_cmd cmd {PF_INVALID};
    int msgid {UnsetInt};
    int refid {UnsetInt};
    port_forward_status status {PFS_INVALID};
    std::string text {UnsetString};
    std::string key {UnsetString};
    json data;

    static int _msgid;

    port_forward_message() : msgid {_msgid++} {}

    JSON_DEFINE_TYPE_INTRUSIVE(port_forward_message,
                               cmd,
                               msgid,
                               refid,
                               status,
                               text,
                               key,
                               data);
};
int port_forward_message::_msgid = 1;

namespace vxg {
namespace cloud {
class reverse_tunneling {
    vxg::logger::logger_ptr logger = vxg::logger::instance("reverse-tunneling");
    struct tunnel {
        typedef std::shared_ptr<tunnel> ptr;

        std::string client_id;
        libssh2::reverse_tunnel::ptr ssh;
        std::chrono::time_point<std::chrono::system_clock> creation_time_;
        std::chrono::seconds timeout;
    };

    std::map<std::string, std::pair<std::string, std::string>> keypairs_;
    std::map<std::string, tunnel::ptr> tunnels_;
    std::thread thread_gc_;
    std::mutex lock_;
    std::atomic<bool> running_ {true};

    reverse_tunneling() { thread_gc_ = std::thread(_gc, this); }

    static bool _expired(tunnel::ptr tunnel) {
        return (std::chrono::system_clock::now() - tunnel->creation_time_ >
                tunnel->timeout);
    }

    static void _gc(reverse_tunneling* rt) {
        while (rt->running_) {
            {
                std::lock_guard<std::mutex> lock(rt->lock_);

                for (auto t = rt->tunnels_.cbegin();
                     t != rt->tunnels_.cend();) {
                    if (_expired(t->second)) {
                        t->second->ssh->stop();
                        rt->tunnels_.erase(t++);
                    } else
                        t++;
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

public:
    ~reverse_tunneling() {
        running_ = false;
        if (thread_gc_.joinable())
            thread_gc_.join();
    }

    static reverse_tunneling& instance() {
        static reverse_tunneling rt;
        return rt;
    }

    // /axis-cgi/param.cgi?action=update&root.VXGCloudAgent.AllowReversePortForwarding=yes
    bool handle(std::string client_id, std::string& data) {
        using namespace nlohmann;
        port_forward_message pf_message;
        port_forward_message pf_reply;

        try {
            pf_message = json::parse(data.c_str());
            pf_reply.refid = pf_message.msgid;
        } catch (...) {
            logger->error("Failed to parse command");
            return false;
        }

        logger->info("Port forward command {}", data);

        switch (pf_message.cmd) {
            // 1. Allocate tunnel.
            // 2. Retreive public key from the generated keypair.
            // 3. Send public key to peer.
            case PF_GET_KEY: {
                keypairs_[client_id] =
                    openssl::rsa_keygen::instance().generate_keypair();

                pf_reply.key = openssl::rsa_keygen::instance().public_pem2ssh(
                    keypairs_[client_id].second);
                pf_reply.status = PFS_OK;
            } break;
            // 1. Fill tunnel parameters.
            // 2. Start tunnel.
            // 3. Send reply with status to peer.
            case PF_CREATE_TUNNEL: {
                libssh2::reverse_tunnel::params tunnel_params;
                port_forward_params params = pf_message.data;
                tunnel::ptr complete_tunnel = std::make_shared<tunnel>();
                std::string tunnel_key =
                    params.cam_ip + ":" + std::to_string(params.cam_port);
                std::lock_guard<std::mutex> lock(lock_);

                if (tunnels_.count(tunnel_key)) {
                    logger->error("Tunnel {} already exists", tunnel_key);
                    pf_reply.status = PFS_ERROR;
                    pf_reply.text = "Tunnel " + tunnel_key + " already exists.";
                } else if (!keypairs_.count(client_id)) {
                    logger->error("No keypair exists for client {}", client_id);
                    pf_reply.status = PFS_ERROR;
                    pf_reply.text = "Server should request key first";
                } else {
                    tunnel_params.local_dest_ip = params.cam_ip;
                    tunnel_params.local_dest_port = params.cam_port;
                    tunnel_params.remote_listen_port = params.port_out;
                    tunnel_params.ssh_server_username = params.user;
                    tunnel_params.ssh_server = params.host;
                    tunnel_params.timeout = params.timeout;
                    tunnel_params.ssh_private_key = keypairs_[client_id].first;
                    tunnel_params.ssh_public_key = keypairs_[client_id].second;
                    // No longer need keypair here
                    keypairs_.erase(client_id);

                    auto ssh_tunnel =
                        libssh2::reverse_tunnel::create(tunnel_params);

                    complete_tunnel->creation_time_ =
                        std::chrono::system_clock::now();
                    complete_tunnel->ssh = ssh_tunnel;
                    complete_tunnel->timeout =
                        std::chrono::seconds(params.timeout);
                    complete_tunnel->client_id = client_id;

                    if (ssh_tunnel->start()) {
                        pf_reply.status = PFS_OK;
                        tunnels_[tunnel_key] = complete_tunnel;
                    } else {
                        pf_reply.status = PFS_ERROR;
                    }

                    pf_reply.text = json(ssh_tunnel->get_status()).dump();
                }
            } break;
            case PF_DELETE_TUNNEL: {
                std::lock_guard<std::mutex> lock(lock_);
                if (pf_message.data != nullptr) {
                    port_forward_params params = pf_message.data;
                    std::string victim =
                        params.cam_ip + ":" + std::to_string(params.cam_port);

                    if (tunnels_.count(victim)) {
                        logger->info("Stopping tunnel {}", victim);

                        tunnels_[victim]->ssh->stop();
                        tunnels_.erase(victim);

                        pf_reply.status = PFS_OK;
                        pf_reply.text = "Tunnel closed";
                    } else {
                        pf_reply.status = PFS_ERROR;
                        pf_reply.text = "Tunnel " + victim + " not found";
                    }
                } else {
                    pf_reply.status = PFS_ERROR;
                    pf_reply.text = "Unable to parse data field!";
                }
            } break;
            default: {
                logger->error("Unknown command {}", data);
            }
        }

        // data argument is in/out, fill reply
        data = json(pf_reply).dump();

        logger->info("Port forward command reply {}", data);

        return true;
    }
};
}  // namespace cloud
}  // namespace vxg

#endif
