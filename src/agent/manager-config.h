#ifndef __CAMERAMANAGERCONFIG_H__
#define __CAMERAMANAGERCONFIG_H__

#include <utils/logging.h>

#include <agent/stream.h>
#include <cloud/CloudRegToken.h>
#include <cloud/Uri.h>
#include <utils/profile.h>
#include <utils/utils.h>

// FIXME:
using namespace std;

namespace vxg {
namespace cloud {
namespace agent {
class manager_config {
    vxg::logger::logger_ptr logger {
        vxg::logger::instance("agent-manager-config")};

public:
    string upload_url;
    string media_server;
    string cam_path;
    string ca;
    string sid;
    string pwd;
    string uuid;
    string conn_id;
    long long cam_id {0};
    CloudRegToken reg_token;
    string backward_url;

    // some camera details
    bool activity {false};
    bool streaming {false};
    bool status_led {false};
    string device_ipaddr {"127.0.0.1"};
    string agent_version {"0"};
    string device_brand {"noname"};
    string device_model {"nomodel"};
    string device_serial {"1234567"};
    string device_fw_version {"0"};
    string timezone {"UTC"};
    string device_vendor {"noname"};
    string device_type {"notype"};

    std::string proxy_socks5_uri_;
    std::string proxy_socks4_uri_;

    // WS(S) - 8888(8883)
    // default parameters
    string protocol {"ws://"};
    string default_server_address {"cam.skyvr.videoexpertsgroup.com"};
    string server_address {"cam.skyvr.videoexpertsgroup.com"};
    string reconnect_server_address;
    int port {8888};
    bool ssl_disabled {true};

    proto::access_token access_token_;

    virtual ~manager_config() {};

    manager_config() {
        device_ipaddr = profile::global::instance().device_ipaddr;
        device_brand = profile::global::instance().device_vendor;
        device_model = profile::global::instance().device_model;
        device_serial = profile::global::instance().device_serial;
        device_fw_version = profile::global::instance().device_fw_version;
        agent_version = profile::global::instance().agent_version;
        device_vendor = profile::global::instance().device_vendor;
        device_type = profile::global::instance().device_type;
        ssl_disabled = profile::global::instance().insecure_cloud_channel;

        if (!ssl_disabled) {
            protocol = "wss://";
            port = 8883;
            logger->info("Secure channel");
        } else {
            protocol = "ws://";
            port = 8888;
            logger->warn("Insecure channel");
        }

        timezone = "UTC";
    }

    void set_cm_address(string url) {
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

    string get_cm_address() {
        if (server_address.empty())
            logger->info("get_cm_address address empty");
        else
            logger->info("get_cm_address address={}", server_address.c_str());

        return server_address;
    }

    void set_reconnect_address(string val) {
        reconnect_server_address = val;

        if (reconnect_server_address.empty())
            logger->info(
                "set_reconnect_address reconnect_server_address empty");
        else
            logger->info("set_reconnect_address reconnect_server_address={}",
                         reconnect_server_address.c_str());
    }

    string get_address() {
        logger->info("get_address isEmpty={}", reg_token.isEmpty());

        string uri;
        string address = reconnect_server_address.empty()
                             ? server_address
                             : reconnect_server_address;
        bool noPort = (address.find(":") == string::npos);

        logger->info("get_address address = {}", address.c_str());

        if (!reg_token.isEmpty()) {
            uri = protocol + address +
                  (noPort ? ":" + std::to_string(port) : "") + "/ctl/NEW/" +
                  reg_token.getToken() + "/";
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
        auto rtmp_proto = !media_server.rfind("rtmp")
                              ? ""
                              : ssl_disabled ? "rtmp://" : "rtmps://";

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
}  // namespace agent
}  // namespace cloud
}  // namespace vxg
#endif
