#ifndef __REST_API_H
#define __REST_API_H

#include <algorithm>
#include <memory>

#include <net/http.h>
#include <utils/base64.h>

namespace vxg {
namespace cloud {

//! @brief Helpers for HTTP based APIs
namespace rest_api {

using kv_list = std::map<std::string, std::string>;
using k_list = std::vector<std::string>;

//! @brief Parser abstract class, parses raw data to kv map or kv map to raw
//! string data.
class parser {
public:
    parser() {}
    virtual ~parser() {}

    virtual kv_list process(std::string raw_data) = 0;
    virtual std::string reverse_process(kv_list kv) {
        vxg::logger::warn(
            "Parser's reverse_process() not implemented but used!");
        return "";
    }
};  // class parser

//! Basic kv parser, parses k='v'\r\n... lines into std::map
const std::string __trim_chars = "\n\r\'";
class kv_parser : public parser {
    std::string& trim(std::string& str,
                      const std::string& chars = __trim_chars) {
        for (auto& c : __trim_chars)
            str.erase(std::remove(str.begin(), str.end(), c), str.end());

        return str;
    }

public:
    kv_parser() {}

    virtual ~kv_parser() {}

    virtual kv_list process(std::string raw_data) {
        std::istringstream props(raw_data);
        std::string line, k, v;
        kv_list results;

        while (std::getline(props, line)) {
            std::istringstream sline(line);
            std::getline(sline, k, '=');
            if (line == k)
                continue;
            std::getline(sline, v);

            results[k] = trim(v);
        }

        return results;
    }

    virtual std::string reverse_process(kv_list kvmap) {
        std::string result;

        for (auto kv = kvmap.begin(); kv != kvmap.end(); ++kv) {
            if (std::next(kv) != kvmap.end()) {
                result += kv->first + "=" + kv->second + "&";
            } else {
                result += kv->first + "=" + kv->second;
            }
        }

        return result;
    }
};  // class kv_parser

//! HTTP settings setter/getter class dervied from the kv_parser
class http {
    vxg::logger::logger_ptr logger {vxg::logger::instance("http-rest-api")};
    transport::libwebsockets::http http_transport_;
    std::string base_uri_ = "";
    transport::libwebsockets::http::request::auth_context auth_context_;
    kv_list common_headers_;
    parser* parser_ {nullptr};

public:
    http(std::string base_uri = "http://localhost",
         transport::libwebsockets::http::request::auth_context auth_context =
             transport::libwebsockets::http::request::auth_context(
                 transport::libwebsockets::http::request::AUTH_NONE,
                 "",
                 ""),
         parser* p = new kv_parser)
        : base_uri_ {base_uri}, auth_context_ {auth_context}, parser_ {p} {
        // Start http transport thread
        http_transport_.start();
    }

    virtual ~http() {
        http_transport_.stop();
        delete parser_;
    }

    void reset(
        std::string base_uri,
        transport::libwebsockets::http::request::auth_context auth_context) {
        base_uri_ = base_uri;
        auth_context_ = auth_context;
        common_headers_.clear();
    }

    void reset(std::string base_uri,
               std::string username,
               std::string password) {
        reset(base_uri, transport::libwebsockets::http::request::auth_context(
                            transport::libwebsockets::http::request::AUTH_BASIC,
                            username, password));
    }

    std::string _dump_req_error(transport::libwebsockets::http::request* req) {
        std::string dump = utils::string_format(
            "HTTP '%s' request to '%s' failed, status %d", req->method_.c_str(),
            req->url_.c_str(), req->status_);

        if (req->method_ == "POST" || req->method_ == "PUT") {
            dump += "\nRequest body:\n";
            dump += utils::string_format("%s", req->request_body_.c_str());
        }

        dump += "\nResponse body:\n";
        dump += utils::string_format("%s", req->response_body_.c_str());

        return dump;
    }

    kv_list request(const std::string& path, const std::string& method) {
        kv_list result;
        auto req = std::make_shared<transport::libwebsockets::http::request>(
            base_uri_ + path, method);

        // Add common headers to http requests
        for (auto& kv : common_headers_)
            req->set_header(kv.first, kv.second);

        // Authorization context for request
        req->set_auth_context(auth_context_);

        // Perform sync request
        if (http_transport_.make_blocked(req)) {
            if (req->status_ == 200) {
                logger->trace("http status {}", req->status_);

                logger->trace("http response\n{}", req->response_body_);

                // Don't parse binary data
                if (req->response_headers_.count("content-type")) {
                    if (req->response_headers_["content-type"] ==
                            "application/octet-stream" ||
                        req->response_headers_["content-type"] ==
                            "application/http" ||
                        utils::string_startswith(
                            req->response_headers_["content-type"], "image") ||
                        utils::string_startswith(
                            req->response_headers_["content-type"], "video"))
                        return {};
                }

                if (parser_)
                    result = parser_->process(req->response_body_);
            } else {
                logger->error(_dump_req_error(req.get()));
            }
        } else {
            logger->error("HTTP request connection failed");
        }

        return result;
    }

    struct multipart_context {
        bool multipart {false};
        std::string multipart_form_field_name;
        std::string multipart_upload_filename;
    };

    static multipart_context default_multipart_context() {
        struct multipart_context default_mp_ctx;
        return default_mp_ctx;
    }

    std::shared_ptr<transport::libwebsockets::http::request> request_raw(
        const std::string& path,
        const std::string& method,
        const std::string& body = "",
        const std::string& mime = "",
        struct multipart_context mp_context = default_multipart_context(),
        int ok_code = 200) {
        std::string url = base_uri_ + path;
        auto req = std::make_shared<transport::libwebsockets::http::request>(
            url, method, body);

        // Add common headers to http requests
        for (auto& kv : common_headers_)
            req->set_header(kv.first, kv.second);

        if (!mime.empty()) {
            if (!mp_context.multipart)
                req->set_header("Content-Type", mime);
            else
                req->multipart_content_type_ = mime;
        }

        // Authorization context for request
        req->set_auth_context(auth_context_);

        if (mp_context.multipart) {
            req->multipart_ = true;
            req->multipart_form_field_ = mp_context.multipart_form_field_name;
            req->multipart_upload_filename_ =
                mp_context.multipart_upload_filename;
        }

        logger->trace("HTTP '{}' url '{}' mime '{}' multipart {}", method, url,
                      mime, mp_context.multipart);
        if (!body.empty()) {
            if (mime != "application/octet-stream" &&
                !utils::string_startswith(mime, "audio") &&
                !utils::string_startswith(mime, "video"))
                logger->trace("request body:\n{}", body);
        }

        // Perform sync request
        if (http_transport_.make_blocked(req)) {
            if (req->status_ == ok_code) {
                logger->trace("HTTP status {}", req->status_);
            } else {
                logger->error(_dump_req_error(req.get()));
            }
        } else {
            logger->error("HTTP request connection failed");
            req->status_ = -1;
        }

        return req;
    }

    //! Get keys values HTTP request
    /*!
        \param path HTTP url path
        \param method HTTP method to use
        \param klist List of keys to get, will be used as HTTP query string
        \return Requested key value map
    */
    virtual kv_list get(const std::string& path,
                        const std::string& method,
                        k_list klist) {
        std::string kparams;
        for (auto& k : klist) {
            kparams.append(k);
            kparams.append("&");
        }

        logger->trace("HTTP GET query string: {}", kparams);

        return request(path + kparams, method);
    }

    //! Get key value HTTP request.
    /*!
        \param path HTTP url path.
        \param method HTTP method to use.
        \param k Key to get, will be used as HTTP query string.
        \return Requested key value.
    */
    virtual std::string get(const std::string& path,
                            const std::string& method,
                            const std::string& k) {
        return request(path + k, method)[k];
    }

    //! Set key value HTTP request.
    /*!
        \param path HTTP url path.
        \param method HTTP method to use.
        \param k Key to set, will be used as HTTP query string.
        \param v Value to set, will be used as HTTP query string.
    */
    virtual void set(const std::string& path,
                     const std::string& method,
                     const std::string& k,
                     const std::string& v) {
        if ((method == "POST" || method == "PUT") && !k.empty())
            request_raw(path, method, k + "=" + v);
        else
            request(path + k + "=" + v, method);
    }

    //! Set mapped keys values HTTP request
    /*!
        \param path HTTP url path
        \param method HTTP method to use
        \param kvmap KV map to set, will be used as HTTP query string
    */
    virtual void set(const std::string& path,
                     const std::string& method,
                     kv_list& kvmap) {
        std::string kparams;
        for (auto& kv : kvmap) {
            kparams.append(kv.first);
            kparams.append("=");
            kparams.append(kv.second);
            kparams.append("&");
        }
        nlohmann::json j = kvmap;

        logger->trace("HTTP SET query string:\n{}", j.dump(2));

        if ((method == "POST" || method == "PUT") && !kparams.empty())
            request_raw(path, method, kparams);
        else
            request(path + kparams, method);
    }
};  // class http

}  // namespace rest_api
}  // namespace cloud
}  // namespace vxg

#endif  // __REST_API_H