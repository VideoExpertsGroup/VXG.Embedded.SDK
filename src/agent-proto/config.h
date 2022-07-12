#pragma once

#include <config.h>

#include <agent-proto/objects/config.h>
#include <agent/config.h>

namespace vxg {
namespace cloud {
namespace agent {
namespace proto {
class config {
    vxg::logger::logger_ptr logger {vxg::logger::instance("proto-config")};

public:
    std::string upload_url;
    std::string media_server;
    std::string cam_path;
    std::string ca;
    std::string sid;
    std::string pwd;
    std::string uuid;
    std::string conn_id;
    long long cam_id {0};
    std::string backward_url;
    bool raw_messaging;

    // some camera details
    bool activity {false};
    bool streaming {false};
    bool status_led {false};
    std::string device_ipaddr {"127.0.0.1"};
    std::string agent_version {"0"};
    std::string device_brand {"noname"};
    std::string device_model {"nomodel"};
    std::string device_serial {"1234567"};
    std::string device_fw_version {"0"};
    std::string timezone {"UTC"};
    std::string device_vendor {"noname"};
    std::string device_type {"notype"};
    std::string proxy_socks5_uri_;
    std::string proxy_socks4_uri_;

    // WS(S) - 8888(8883)
    // default parameters
    std::string protocol {"ws://"};
    std::string default_server_address {DEFAULT_CLOUD_TOKEN_CAM};
    std::string server_address {DEFAULT_CLOUD_TOKEN_CAM};
    std::string reconnect_server_address;
    int port {DEFAULT_CLOUD_TOKEN_CAM_P};
    bool ssl_disabled {true};

    proto::access_token access_token_;

    config(const agent::config& manager_config,
           const proto::access_token& access_token) {
        device_ipaddr = manager_config.device_ipaddr;
        device_brand = manager_config.device_vendor;
        device_model = manager_config.device_model;
        device_serial = manager_config.device_serial;
        device_fw_version = manager_config.device_fw_version;
        agent_version = manager_config.agent_version;
        device_vendor = manager_config.device_vendor;
        device_type = manager_config.device_type;
        ssl_disabled = manager_config.insecure_cloud_channel;
        sid = manager_config.cm_registration_sid;
        raw_messaging = manager_config.raw_messaging;

        access_token_ = access_token;

        if (!ssl_disabled) {
            protocol = "wss://";
            port = access_token_.cam_sp;
            logger->info("Secure Cloud connection channel used");
        } else {
            protocol = "ws://";
            port = access_token_.cam_p;
            logger->warn("Insecure Cloud connection channel used");
        }

        if (!__is_unset(access_token.proxy.socks5)) {
            logger->info("Access token contains sock5 proxy {}",
                         access_token.proxy.socks5);
            proxy_socks5_uri_ = access_token.proxy.socks5;
        }

        if (!__is_unset(access_token.proxy.socks4)) {
            logger->info("Access token contains socks4 proxy {}",
                         access_token.proxy.socks4);
            proxy_socks4_uri_ = access_token.proxy.socks4;
        }

        timezone = "UTC";
    }
    virtual ~config() {}

    void set_cm_address(std::string url) {
        if (!ssl_disabled) {
            protocol = "wss://";
            port = 8883;
            logger->info("Secure channel");
        } else {
            protocol = "ws://";
            port = 8888;
            logger->warn("Insecure channel");
        }

        if (url.empty()) {
            server_address = default_server_address;
            logger->info("set_cm_address default_server_address={}",
                         default_server_address.c_str());
        } else {
            server_address = url;
            logger->info("set_cm_address url={}", url.c_str());
        }

        if (server_address.empty())
            logger->info("set_cm_address address empty");
        else
            logger->info("set_cm_address address={}", server_address.c_str());
    }

    std::string get_cm_address() {
        if (server_address.empty())
            logger->info("get_cm_address address empty");
        else
            logger->info("get_cm_address address={}", server_address.c_str());

        return server_address;
    }

    void set_reconnect_address(std::string val) {
        reconnect_server_address = val;

        if (reconnect_server_address.empty())
            logger->info(
                "set_reconnect_address reconnect_server_address empty");
        else
            logger->info("set_reconnect_address reconnect_server_address={}",
                         reconnect_server_address.c_str());
    }

    std::string get_address() {
        logger->info("get_address isEmpty={}", access_token_.token.empty());

        std::string uri;
        std::string address = reconnect_server_address.empty()
                                  ? server_address
                                  : reconnect_server_address;
        bool noPort = (address.find(":") == std::string::npos);

        logger->info("get_address address = {}", address.c_str());

        if (!access_token_.token.empty()) {
            uri = protocol + address +
                  (noPort ? ":" + std::to_string(port) : "") + "/ctl/NEW/" +
                  access_token_.token + "/";
        } else if (!conn_id.empty()) {
            uri = protocol + address +
                  (noPort ? ":" + std::to_string(port) : "") + "/ctl/" +
                  conn_id + "/";
        } else {
            logger->error("get_address, error");
        }

        logger->info("get_address {}", uri.c_str());

        return uri;
    }

    enum url_type {
        UT_UNKNOWN,
        UT_PROTO,
        UT_UPLOAD_RECORD,
        UT_UPLOAD_PREVIEW,
        UT_UPLOAD_TEXT,
        UT_STREAM_LIVE,
        UT_STREAM_BACKWARD,
    };

    std::string buildUrl(url_type t,
                         std::string stream_name = "",
                         std::string publish_session_id = "") {
        std::string url;
        auto now = utils::time::now_ISO8601_UTC_packed();
        auto rtmp_proto = !media_server.rfind("rtmp") ? ""
                          : ssl_disabled              ? "rtmp://"
                                                      : "rtmps://";
        switch (t) {
            case UT_STREAM_LIVE: {
                url = rtmp_proto + media_server + "/" + cam_path + stream_name +
                      "?sid=" + sid;
                if (!publish_session_id.empty())
                    url += "&psid=" + publish_session_id;

                if (!proxy_socks4_uri_.empty())
                    url += " socks=" + proxy_socks4_uri_;
            } break;
            case UT_STREAM_BACKWARD: {
                url = backward_url;
            } break;
            case UT_UPLOAD_PREVIEW: {
                url = upload_url + "/" + cam_path + "?sid=" + sid +
                      "&cat=preview&type=jpg&start=" + now;
            } break;
            case UT_UPLOAD_TEXT: {
                url = upload_url + "/" + cam_path + "?sid=" + sid +
                      "&cat=log&type=txt&start=" + now;
            } break;
            case UT_PROTO: {
                url = access_token_.cam_base_uri(!ssl_disabled,
                                                 reconnect_server_address) +
                      "/ctl/NEW/" + access_token_.token + "/";
            } break;
            default:
                logger->error("Unable to build url for type {}", t);
                break;
        }

        return url;
    }

    bool parse_proxy(std::string& host,
                     std::string& username,
                     std::string& password,
                     std::string& port) {
        if (!proxy_socks5_uri_.empty()) {
            // socks5://login:password@socks-proxy.example.com
            Uri parsed_uri = Uri::Parse(proxy_socks5_uri_);
            auto tokens = utils::string_split(proxy_socks5_uri_, ':');
            return true;
        }
        return false;
    }
};

}  // namespace proto
}  // namespace agent
}  // namespace cloud
}  // namespace vxg