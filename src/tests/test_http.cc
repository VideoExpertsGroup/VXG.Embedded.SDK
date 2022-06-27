#include <gtest/gtest.h>
#include <net/http.h>

#include <utils/base64.h>

#include <regex>

#include <xml2json.hpp>

using namespace ::testing;
using namespace std;
using namespace vxg::cloud::transport::libwebsockets;

class TestHTTPAuth : public Test {
protected:
    TestHTTPAuth() {
        uri_ =
            "http://CAMERA/axis-cgi/applications/"
            "control.cgi?action=start&package=vmd";
        username_ = "root";
        password_ = "XpDAmLFC";
        method_ = "GET";
        uri_ =
            "http://CAMERA/axis-cgi/"
            "param.cgi?action=update&root.Image.I0.Appearance.Resolution="
            "1920x1080&root.Image.I0.MPEG.PCount=10&root.Image.I0.RateControl."
            "Mode=cbr&root.Image.I0.RateControl.TargetBitrate=1000&root.Image."
            "I0.Stream.FPS=20";
        uri_ =
            "http://CAMERA/axis-cgi/jpg/"
            "image.cgi?resolution=1920x1080";

        http_ = std::make_shared<http>();
        // http_->set_proxy("socks5://54.221.121.236:13070");
    }

    virtual void SetUp() { http_->start(); }

    virtual void TearDown() { http_->stop(); }

public:
    std::shared_ptr<http> http_;
    std::string uri_;
    std::string method_;
    std::string username_;
    std::string password_;
    std::string proxy_;
    std::mutex lock_;
    std::condition_variable cond_;
    bool done_ {false};

    std::mutex lock2_;
    std::condition_variable cond2_;
    bool done2_ {false};
};

TEST_F(TestHTTPAuth, NONE) {
    auto req = std::make_shared<http::request>(uri_, method_);
    bool ret = http_->make_blocked(req);

    EXPECT_EQ(ret, true);
    EXPECT_EQ(req->status_, 403);
}

std::map<std::string, std::string> _parse_auth(std::string auth_line) {
    std::smatch mat_opt, mat_val;
    std::map<std::string, std::string> result;

    try {
        std::regex rex_opt(R"(\s*([A-Za-z]{3,})\s*=)");
        std::regex rex_val(
            "=\\s*((?:[^,\"]|\"\\s*([^\"]*?)\\s?\")+?)(?=\\s*,|$)");

        auto& str = auth_line;
        while (std::regex_search(auth_line, mat_opt, rex_opt)) {
            if (mat_opt.size() >= 2) {
                auto field = mat_opt[1].str();

                if (std::regex_search(auth_line, mat_val, rex_val)) {
                    auto value = mat_val[2].matched ? mat_val[2].str()
                                                    : mat_val[1].str();
                    result[field] = value;
                }

                str = mat_opt.suffix().str();
            }
        }

        for (auto& itr : result) {
            std::cout << itr.first << ":" << itr.second << ".\n";
        }
    } catch (std::regex_error& e) {
        std::cout << "regex_search failed" << e.what() << "\n";
    }

    return result;
}

#ifdef LWS_SSL
TEST_F(TestHTTPAuth, BASIC) {
    auto req = std::make_shared<http::request>(uri_, method_);
    auto auth_ctx = http::request::auth_context(http::request::AUTH_BASIC,
                                                username_, password_);
    req->set_auth_context(auth_ctx);

    bool ret = http_->make_blocked(req);

    spdlog::info("{}", req->response_body_);

    spdlog::info("Auth {}", req->response_headers_["www-authenticate"]);
    // req->response_headers_["www-authenticate"] =
    //     "Digest realm=\"AXIS_ACCC8E023272\", "
    //     "nonce=\"+UadMZG1BQA=0f0b971cba5421add1eed0f72e237d88d96f8cd4\", "
    //     "algorithm=MD5, qop=\"auth\"";
    auto auth_map = _parse_auth(req->response_headers_["www-authenticate"]);

    for (auto& h : auth_map) {
        spdlog::info("{} : {}", h.first, h.second);
    }

    struct lws_genhash_ctx hash;
    uint8_t hash1[lws_genhash_size(LWS_GENHASH_TYPE_MD5)];
    char hash1_hex[lws_genhash_size(LWS_GENHASH_TYPE_MD5) * 2 + 1];
    uint8_t hash2[lws_genhash_size(LWS_GENHASH_TYPE_MD5)];
    char hash2_hex[lws_genhash_size(LWS_GENHASH_TYPE_MD5) * 2 + 1];
    uint8_t hash_resp[lws_genhash_size(LWS_GENHASH_TYPE_MD5)];
    uint32_t cnonce_int;
    uint32_t nc_int = 1;
    char cnonce[8 + 1];
    char nc[8 + 1];
    char hash_resp_hex[lws_genhash_size(LWS_GENHASH_TYPE_MD5) * 2 + 1];
    auto nonce_decoded = auth_map["nonce"];
    std::string cnonce_ = "MWUwN2E4ZTNiOGM5OTk4M2FlNzYzZjU5N2FjYmU1MTc=";
    std::string digest_uri = "/axis-cgi/param.cgi";

    // cnonce
    cnonce_int = __bswap_32(0x0164595f);  // rand();
    lws_byte_array_to_hex((uint8_t*)&cnonce_int, sizeof(cnonce_int), cnonce,
                          sizeof(cnonce));
    cnonce[sizeof(cnonce) - 1] = '\0';
    cnonce_ = cnonce;
    nc_int = 1;
    std::string ncs = "00000001";

    // HASH1
    lws_genhash_init(&hash, LWS_GENHASH_TYPE_MD5);
    auto hash1_str = req->auth_context_.username + ":" + auth_map["realm"] +
                     ":" + req->auth_context_.password;
    lws_genhash_update(&hash, hash1_str.c_str(), hash1_str.size());
    lws_genhash_destroy(&hash, hash1);
    lws_byte_array_to_hex((uint8_t*)hash1, sizeof(hash1), hash1_hex,
                          sizeof(hash1_hex));

    // HASH2
    auto hash2_str = req->method_ + ":" + digest_uri;
    lws_genhash_init(&hash, LWS_GENHASH_TYPE_MD5);
    lws_genhash_update(&hash, hash2_str.c_str(), hash2_str.size());
    lws_genhash_destroy(&hash, hash2);
    lws_byte_array_to_hex((uint8_t*)hash2, sizeof(hash2), hash2_hex,
                          sizeof(hash2_hex));

    // Response
    std::string resp;
    resp.insert(resp.end(), hash1_hex, hash1_hex + sizeof(hash1_hex) - 1);
    resp.insert(resp.end(), ':');
    resp.insert(resp.end(), nonce_decoded.begin(), nonce_decoded.end());
    resp.insert(resp.end(), ':');
    resp.insert(resp.end(), ncs.begin(), ncs.end());
    resp.insert(resp.end(), ':');
    resp.insert(resp.end(), cnonce_.begin(), cnonce_.end());
    resp.insert(resp.end(), ':');
    resp.insert(resp.end(), auth_map["qop"].begin(), auth_map["qop"].end());
    resp.insert(resp.end(), ':');
    resp.insert(resp.end(), hash2_hex, hash2_hex + sizeof(hash2_hex) - 1);

    spdlog::info("RESPONSE: {}", resp);

    lws_genhash_init(&hash, LWS_GENHASH_TYPE_MD5);
    lws_genhash_update(&hash, resp.data(), resp.size());
    lws_genhash_destroy(&hash, hash_resp);

    lws_byte_array_to_hex(hash_resp, sizeof(hash_resp), hash_resp_hex,
                          sizeof(hash_resp_hex));
    hash_resp_hex[sizeof(hash_resp_hex) - 1] = '\0';

    // HTTP request with auth header
    std::string auth_fmt =
        "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", "
        "qop=%s, nc=%08x, cnonce=\"%s\", response=\"%s\", algorithm=\"MD5\"";
    std::string www_auth_resp_header = vxg::cloud::utils::string_format(
        auth_fmt, req->auth_context_.username.c_str(),
        auth_map["realm"].c_str(), auth_map["nonce"].c_str(),
        digest_uri.c_str(), auth_map["qop"].c_str(), nc_int, cnonce_.c_str(),
        hash_resp_hex);

    spdlog::info("Auth header: {}", www_auth_resp_header);

    req.reset();
    req = std::make_shared<http::request>(uri_, method_);

    req->set_header("Authorization", www_auth_resp_header);

    ret = http_->make_blocked(req);

    spdlog::info("{}", req->response_body_);

    EXPECT_EQ(ret, true);
    EXPECT_EQ(req->status_, 200);
}
#endif
TEST_F(TestHTTPAuth, DIGEST) {
    spdlog::default_logger()->set_level(spdlog::level::trace);

    auto req = std::make_shared<http::request>(uri_, method_, "",
                                               std::chrono::seconds(220));
    auto auth_ctx = http::request::auth_context(http::request::AUTH_DIGEST,
                                                username_, password_);
    req->set_auth_context(auth_ctx);

    bool ret = http_->make_blocked(req);

    // spdlog::info("{}", req->response_body_);

    EXPECT_EQ(ret, true);
    EXPECT_EQ(req->status_, 200);
}

TEST_F(TestHTTPAuth, Audio) {
    uri_ =
        "http://CAMERA/axis-cgi/"
        "param.cgi?action=listdefinitions&listformat=xmlschema&responseformat="
        "rfc&responsecharset=utf8&group=root.Audio";
    auto req = std::make_shared<http::request>(uri_, "GET");
    auto auth_ctx = http::request::auth_context(http::request::AUTH_DIGEST,
                                                username_, password_);
    req->set_auth_context(auth_ctx);

    bool ret = http_->make_blocked(req);

    auto j = json::parse(xml2json(req->response_body_.c_str()));
    spdlog::info("{}", j.dump(2));

    EXPECT_EQ(ret, true);
    EXPECT_EQ(req->status_, 200);
}

TEST_F(TestHTTPAuth, UploadMulti) {
    uri_ = "http://195.60.68.14:12010/axis-cgi/applications/upload.cgi";
    // uri_ = "http://localhost:8001/axis-cgi/applications/upload.cgi";
    auto req = std::make_shared<http::request>(uri_, "POST");
    auto auth_ctx = http::request::auth_context(http::request::AUTH_DIGEST,
                                                username_, password_);
    req->set_auth_context(auth_ctx);

    std::ifstream log_stream(
        "/home/ilya/work/axis/acap-package/VX_Cloud_Agent_2_1_11_armv7hf.eap",
        std::ios::in | std::ios::binary);
    std::string data;
    if (!log_stream.fail()) {
        data.insert(data.end(), (std::istreambuf_iterator<char>(log_stream)),
                    std::istreambuf_iterator<char>());
        log_stream.close();
    }

    req->request_body_ = data;
    req->multipart_ = true;
    bool ret = http_->make_blocked(req);

    spdlog::info("{}", req->response_body_);

    EXPECT_EQ(ret, true);
    EXPECT_EQ(req->status_, 200);
}

TEST_F(TestHTTPAuth, FileUploadAsync) {
    auto req =
        std::make_shared<http::request>("http://localhost:1234/post2", "POST");
    std::string body_;
    body_.append((1024 * 1024 * 512), 'c');
    req->request_body_ = std::move(body_);

    req->set_response_cb([=](std::string resp, int code) {
        std::unique_lock<std::mutex> lock(lock_);

        if (code == 200)
            vxg::logger::info("HTTP request OK");
        else
            vxg::logger::error("HTTP request FAIL");

        done_ = true;
        cond_.notify_all();
    });

    bool ret = http_->make(req);

    {
        std::unique_lock<std::mutex> lock(lock_);
        cond_.wait(lock, [this] { return done_; });
    }

    vxg::logger::info("Transfer finished, speed {:.3f}MB/s",
                      req->upload_speed / (double)(1024 * 1024));

    EXPECT_EQ(ret, true);
    EXPECT_EQ(req->status_, 200);
}

TEST_F(TestHTTPAuth, FileUploadAsyncConcurent) {
    bool ret = true;

    http_->schedule_timed_cb(
        [=]() {
            auto req1 = std::make_shared<http::request>(
                "http://192.168.100.3:1234/post2?1", "POST");
            std::string body_;
            body_.append((1024 * 1024 * 100), 'c');
            req1->request_body_ = std::move(body_);

            req1->set_response_cb([=](std::string resp, int code) {
                std::unique_lock<std::mutex> lock(lock_);

                if (code == 200) {
                    vxg::logger::info("HTTP request OK");
                    vxg::logger::info(
                        "Transfer 1 finished, speed {:.3f}MB/s",
                        req1->upload_speed / (double)(1024 * 1024));
                } else
                    vxg::logger::error("HTTP request FAIL");

                done_ = true;
                cond_.notify_all();
            });
            http_->make(req1);
        },
        1);

    ///////////////////////////////
    // std::this_thread::sleep_for(std::chrono::seconds(1));
    // http_->schedule_timed_cb([=]() {
    //     auto req2 = std::make_shared<http::request>(
    //         "http://localhost:1234/post2?2", "POST");
    //     std::string body_;
    //     body_.append((1024 * 1024 * 10), 'x');
    //     req2->request_body_ = std::move(body_);

    //     req2->set_response_cb([=](std::string resp, int code) {
    //         std::unique_lock<std::mutex> lock(lock2_);

    //         if (code == 200) {
    //             vxg::logger::info("HTTP request OK");
    //             vxg::logger::info("Transfer 2 finished, speed {:.3f}MB/s",
    //                             req2->upload_speed / (double)(1024 * 1024));
    //         }
    //         else
    //             vxg::logger::error("HTTP request FAIL");

    //         done2_ = true;
    //         cond2_.notify_all();
    //     });
    //     http_->make(req2);
    // }, 1);

    {
        std::unique_lock<std::mutex> lock(lock_);
        cond_.wait(lock, [this] { return done_; });
    }

    // {
    //     std::unique_lock<std::mutex> lock(lock2_);
    //     cond2_.wait(lock, [this] { return done2_; });
    // }

    EXPECT_EQ(ret, true);
    // EXPECT_EQ(req1->status_, 200);
    // EXPECT_EQ(req2->status_, 200);
}
