#pragma once

#include <agent-proto/proto.h>
#include <net/http.h>
#include <utils/logging.h>
#include <utils/profile.h>
#include <utils/utils.h>

#include <cloud/CloudAPIEndPoints.h>
#include <cloud/cloud_api_v4.h>

#include <fstream>
#include <memory>
#include <vector>

#ifdef HAVE_CPP_LIB_FILESYSTEM
#include <experimental/filesystem>
#endif

namespace vxg {
namespace cloud {

struct period {
    cloud::time begin;
    cloud::time end;

    period(cloud::time _begin = utils::time::null(),
           cloud::time _end = utils::time::null())
        : begin {_begin},
          end {_end == utils::time::null() ? utils::time::max() : _end} {}

    period(agent::proto::command::get_direct_upload_url l)
        : begin {utils::time::from_iso_packed(l.file_time)},
          end {utils::time::from_iso_packed(l.file_time) +
               (l.duration_us == cloud::duration(0)
                    ? std::chrono::milliseconds(l.duration)
                    : l.duration_us)} {}

    bool is_open() { return end == utils::time::null(); }
    bool is_null() { return begin == utils::time::null(); }
    bool is_valid() {
        return begin != utils::time::null() && begin != utils::time::max() &&
               end != utils::time::null() && end != utils::time::max();
    }

    bool intersects(const period& r) {
        auto b1 = begin;
        auto b2 = r.begin;
        auto e1 = end != utils::time::null() ? end : utils::time::max();
        auto e2 = r.end != utils::time::null() ? r.end : utils::time::max();

        if (b1 < e2 && e1 > b2)
            return true;
        return false;
    }

    void clear() {
        begin = utils::time::null();
        end = utils::time::null();
    }

    cloud::time::duration duration() { return end - begin; }

    bool operator<(const period& r) { return begin < r.begin; }
};

class timed_storage {
public:
    timed_storage() {}
    virtual ~timed_storage() {}

    struct item : public period {
        enum class data_state { empty, loaded, async_ready };
        std::vector<uint8_t> data;
        data_state state {data_state::empty};
        agent::proto::command::upload_category category;
        agent::proto::command::media_type media_type;

        item(cloud::time begin = utils::time::null(),
             cloud::time end = utils::time::null(),
             std::vector<uint8_t> data = std::vector<uint8_t>())
            : period(begin, end), data {data} {
            state = data.empty() ? data_state::empty : data_state::loaded;
        }

        item(period p, std::vector<uint8_t> data = std::vector<uint8_t>())
            : period(p), data {data} {
            state = data.empty() ? data_state::empty : data_state::loaded;
        }

        item(std::vector<uint8_t>&& data) : data {std::move(data)} {
            state = data.empty() ? data_state::empty : data_state::loaded;
        }

        void clear() {
            period::clear();
            data.clear();
            state = data_state::empty;
        }

        bool empty() { return data.empty(); }

        // bool load() = 0;
        // std::ostream stream() = 0;

        bool operator<(const item& r) { return begin < r.begin; }
    };
    typedef std::shared_ptr<struct item> item_ptr;

    virtual std::vector<item_ptr> list(cloud::time start, cloud::time stop) = 0;
    virtual bool load(item_ptr) = 0;
    virtual bool store(item_ptr) = 0;

    using async_store_finished_cb = std::function<void(bool)>;
    using async_store_is_canceled_cb = std::function<bool(void)>;
    virtual bool store_async(item_ptr,
                             async_store_finished_cb finished_cb,
                             async_store_is_canceled_cb is_canceled_cb) {
        finished_cb(false);
        return false;
    }
    virtual void erase(item_ptr) = 0;

    virtual bool init() { return true; }
    virtual void finit() {}
};
inline bool operator<(const timed_storage::item_ptr l,
                      const timed_storage::item_ptr r) {
    return l->begin < r->begin;
}
typedef std::shared_ptr<timed_storage> timed_storage_ptr;

#ifdef HAVE_CPP_LIB_FILESYSTEM
class vfs_storage : public timed_storage {
    vxg::logger::logger_ptr logger {vxg::logger::instance("vfs_storage")};

    std::experimental::filesystem::path p_;

    bool _period_from_filename(std::string filename, period& p) {
        using namespace vxg::cloud;
        auto timestamps = utils::string_split(filename, '_');

        p.begin = p.end = utils::time::null();

        if (timestamps.size() == 2) {
            p.begin = utils::time::from_iso_packed(timestamps[0]);
            p.end = utils::time::from_iso_packed(timestamps[1]);
        }

        if (!p.is_valid()) {
            // logger->trace("Can't parse timestamps from '{}'", filename);
            return false;
        }

        return true;
    }

    bool _filename_from_period(period p, std::string& filename) {
        namespace fs = std::experimental::filesystem;
        using namespace utils::time;

        if (p.is_null() || p.is_open())
            return false;

        filename =
            fs::path(to_iso_packed(p.begin) + "_" + to_iso_packed(p.end));

        return true;
    }

    bool _load_file(std::string filename, std::vector<uint8_t>& data) {
        std::ifstream f(p_ / filename);
        std::vector<uint8_t> v;
        size_t len = 0;
        bool result = false;

        if (!f.fail()) {
            f.seekg(0, std::ios::end);
            len = f.tellg();
            f.seekg(0, std::ios::beg);

            try {
                data = std::vector<uint8_t>(len);
                f.read((char*)data.data(), len);
                result = true;
            } catch (...) {
                // vector constructor may throw
                logger->error("Unable to allocate memory for file {}",
                              filename);
            }
            f.close();
        }
        return result;
    }

    bool _store_file(std::string filename, const std::vector<uint8_t>& data) {
        std::ofstream f(p_ / filename);
        std::vector<uint8_t> v;
        size_t len = 0;
        bool result = false;

        if (!f.fail()) {
            len = data.size();

            try {
                f.write((char*)data.data(), len);
                result = true;
            } catch (...) {
                // vector constructor may throw
                logger->error("Unable to store data into the file {}",
                              filename);
            }
            f.close();
        }
        return result;
    }

public:
    vfs_storage(std::string root_path) {
        namespace fs = std::experimental::filesystem;

        if (!fs::is_directory(root_path))
            fs::create_directory(root_path);

        p_ = root_path;
    }
    virtual ~vfs_storage() {}

    struct file : public item {
        file(std::experimental::filesystem::path path, period p)
            : p_ {path}, timed_storage::item(p) {}
        std::experimental::filesystem::path p_;
    };
    typedef std::shared_ptr<file> file_ptr;

    virtual std::vector<item_ptr> list(cloud::time start,
                                       cloud::time stop) override {
        namespace fs = std::experimental::filesystem;

        std::vector<item_ptr> result;

        for (const auto& entry : fs::directory_iterator(p_)) {
            period p;

            if (_period_from_filename(entry.path().filename(), p) &&
                p.intersects({start, stop})) {
                result.push_back(std::make_shared<item>(p));
            }
        }

        std::sort(result.begin(), result.end(),
                  [](const item_ptr& l, const item_ptr& r) {
                      return (l->begin < r->begin);
                  });

        return result;
    }

    virtual bool load(item_ptr item) override {
        std::string filename;

        if (_filename_from_period({item->begin, item->end}, filename))
            return _load_file(filename, item->data);

        return false;
    }

    virtual bool store(item_ptr i) override {
        std::string filename;

        if (_filename_from_period({i->begin, i->end}, filename)) {
            logger->debug("Storage item {} - {} successfully stored to {}",
                          utils::time::to_iso_packed(i->begin),
                          utils::time::to_iso_packed(i->end), filename);
            return _store_file(filename, i->data);
        }

        return false;
    }

    virtual void erase(item_ptr) {}
};
#endif
class cloud_storage : public timed_storage {
    vxg::logger::logger_ptr logger {vxg::logger::instance("cloud-storage")};
    vxg::cloud::agent::proto::access_token token_;
    vxg::cloud::transport::libwebsockets::http::ptr http_;
    bool secure_transport_ {true};

public:
    cloud_storage(const agent::proto::access_token& token,
                  transport::libwebsockets::http::ptr http = nullptr)
        : token_ {token}, http_ {http} {
        if (http_ == nullptr)
            http_ = std::make_shared<transport::libwebsockets::http>();

        // Use proxy if access token contains it
        if (!__is_unset(token.proxy.socks5))
            http_->set_proxy(token.proxy.socks5);

        secure_transport_ =
            http_->is_secure() &&
            !agent::profile::global::instance().insecure_cloud_channel;

        http_->start();
    }

    virtual ~cloud_storage() { http_->stop(); }

    virtual std::vector<item_ptr> list(cloud::time start,
                                       cloud::time stop) override {
        namespace lws = vxg::cloud::transport::libwebsockets;
        using namespace vxg::cloud::utils::time;

        std::vector<item_ptr> result;
        api::v2::storage::data::response r;
        do {
            std::string url =
                token_.api_uri(secure_transport_) +
                api::v2::storage::data::ENDPOINT + "token=" + token_.token +
                "&start=" + to_iso2(start) + "&end=" + to_iso2(stop);

            if (!__is_unset(r.meta.next))
                url = token_.api_uri(secure_transport_) + r.meta.next;

            lws::http::request_ptr req =
                std::make_shared<lws::http::request>(url);

            if (http_->make_blocked(req) && req->code() == 200) {
                r = json::parse(req->response_body_);

                for (auto& o : r.objects) {
                    result.push_back(
                        std::make_shared<item>(period(o.start, o.end)));
                }
            } else {
                vxg::logger::error("{}", req->response_body_);
            }
        } while (!__is_unset(r.meta.next));

        return result;
    }

    virtual bool load(item_ptr item) override {
        namespace lws = vxg::cloud::transport::libwebsockets;
        using namespace vxg::cloud::utils::time;

        api::v2::storage::data::response r;
        std::string url =
            token_.api_uri(secure_transport_) +
            api::v2::storage::data::ENDPOINT + "token=" + token_.token +
            "&start=" + to_iso2(item->begin) + "&end=" + to_iso2(item->end);
        lws::http::request_ptr req = std::make_shared<lws::http::request>(url);

        if (http_->make_blocked(req) && req->code() == 200) {
            r = json::parse(req->response_body_);

            if (!r.objects.empty()) {
                lws::http::request_ptr req =
                    std::make_shared<lws::http::request>(r.objects.back().url);

                item->clear();
                if (http_->make_blocked(req) && req->code() == 200) {
                    item->clear();
                    item->begin = r.objects.back().start;
                    item->end = r.objects.back().end;
                    item->data.insert(item->data.end(),
                                      req->response_body_.begin(),
                                      req->response_body_.end());
                    return true;
                } else {
                    vxg::logger::error("{}", req->response_body_);
                }
            }
        } else {
            vxg::logger::error("{}", req->response_body_);
        }

        return false;
    }

    bool store(item_ptr item) {
        namespace lws = vxg::cloud::transport::libwebsockets;
        using namespace vxg::cloud::utils::time;
        using namespace api::v4::storage::records;
        using namespace std::chrono;

        json jreq = json::array();
        upload::request req_data;

        if (item->is_null() || item->is_open() || item->data.empty()) {
            logger->error("Bad storage item: period {} - {}, size {}!",
                          to_iso2(item->begin), to_iso2(item->end),
                          item->data.size());
            return false;
        }

        req_data.time = item->begin;
        req_data.duration =
            duration_cast<cloud::time_spec::precision>(item->end - item->begin)
                .count();
        req_data.size = item->data.size();

        jreq.push_back(req_data);

        // Request upload urls
        lws::http::request_ptr req = std::make_shared<lws::http::request>(
            token_.api_uri(secure_transport_) + upload::ENDPOINT);
        req->method_ = "POST";
        req->request_body_ = jreq.dump(2);

        req->set_header("Authorization", "Acc " + token_.pack());

        if (http_->make_blocked(req) && req->code() == 200) {
            upload::response r = json::parse(req->response_body_);

            if (!r.urls.empty()) {
                lws::http::request_ptr req =
                    std::make_shared<lws::http::request>(r.urls[0]);
                req->method_ = "PUT";
                req->set_header("Content-Type", "video/mp4");
                req->request_body_.insert(req->request_body_.end(),
                                          item->data.begin(), item->data.end());
                if (!http_->make_blocked(req) || req->code() != 200) {
                    vxg::logger::error(
                        "Failed to upload item {} - {}, size {}!",
                        to_iso2(item->begin), to_iso2(item->end),
                        item->data.size());
                    vxg::logger::error("{} {}", req->code(),
                                       req->response_body_);
                } else {
                    logger->info(
                        "Item {} - {} of {} bytes uploaded in {:.3f} seconds "
                        "with speed {:.3f} Kb/s",
                        to_iso2(item->begin), to_iso2(item->end),
                        item->data.size(),
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            req->time_stop - req->time_start)
                                .count() /
                            (double)1000,
                        req->upload_speed / (double)(1024 * 1024));
                    return true;
                }
            }
        } else {
            vxg::logger::error("Failed to request upload urls:\n{}",
                               req->response_body_);
        }

        return false;
    }

    virtual void erase(item_ptr) {}
};

template <class T>
class timeline {
    std::shared_ptr<T> storage_;

public:
    timeline(const vxg::cloud::agent::proto::access_token& access_token,
             transport::libwebsockets::http::ptr http = nullptr)
        : storage_ {std::make_shared<T>(access_token, http)} {}
    timeline(std::string path) : storage_ {std::make_shared<T>(path)} {}

    std::vector<period> _squash_periods(
        std::vector<timed_storage::item_ptr> periods) {
        using namespace utils::time;
        std::vector<period> result;

        if (!periods.empty()) {
            // Sort by begining of the periods

            std::sort(periods.begin(), periods.end());

            // Merge all intersecting periods
            auto it = periods.begin();
            period current((*it)->begin, (*it)->end);
            it++;
            while (it != periods.end()) {
                if (current.end >= (*it)->begin)
                    current.end = std::max(current.end, (*it)->end);
                else {
                    result.push_back(current);
                    current = {(*it)->begin, (*it)->end};
                }
                it++;
            }

            result.push_back(current);
        }

        return result;
    }

public:
    std::vector<period> slices(cloud::time start, cloud::time stop) {
        return _squash_periods(storage_->list(start, stop));
    }
};

namespace sync {
class timeline {
    std::shared_ptr<timed_storage> storage_;
    vxg::logger::logger_ptr logger {vxg::logger::instance("timeline")};

public:
    timeline(timed_storage_ptr storage) : storage_ {storage} {}
    virtual ~timeline() {}

    std::vector<period> _squash_periods(
        std::vector<timed_storage::item_ptr> periods) {
        using namespace utils::time;
        std::vector<period> result;

        if (!periods.empty()) {
            // Sort by begining of the periods

            std::sort(periods.begin(), periods.end());

            // Merge all intersecting periods
            auto it = periods.begin();
            period current((*it)->begin, (*it)->end);
            it++;
            while (it != periods.end()) {
                if (current.end >= (*it)->begin)
                    current.end = std::max(current.end, (*it)->end);
                else {
                    result.push_back(current);
                    current = {(*it)->begin, (*it)->end};
                }
                it++;
            }

            result.push_back(current);
        }

        return result;
    }

    virtual bool init() { return storage_->init(); }

    virtual void finit() { storage_->finit(); }

public:
    std::vector<period> slices(cloud::time start, cloud::time stop) {
        return _squash_periods(storage_->list(start, stop));
    }

    std::vector<timed_storage::item_ptr> list(cloud::time start,
                                              cloud::time stop) {
        auto result = storage_->list(start, stop);
        std::sort(result.begin(), result.end(),
                  [](const timed_storage::item_ptr& l,
                     const timed_storage::item_ptr& r) {
                      return (l->begin < r->begin);
                  });
        return result;
    }

    bool store(timed_storage::item_ptr item) {
        logger->info("Storage {} storing item {} - {}", (void*)storage_.get(),
                     utils::time::to_iso_packed(item->begin),
                     utils::time::to_iso_packed(item->end));
        return storage_->store(item);
    }

    bool load(timed_storage::item_ptr item) {
        logger->info("Storage {} loading item {} - {}", (void*)storage_.get(),
                     utils::time::to_iso_packed(item->begin),
                     utils::time::to_iso_packed(item->end));
        return storage_->load(item);
    }

    using async_store_finished_cb = std::function<void(bool)>;
    using async_store_is_canceled_cb = std::function<bool(void)>;
    virtual bool store_async(timed_storage::item_ptr item,
                             async_store_finished_cb finished_cb,
                             async_store_is_canceled_cb is_canceled_cb) {
        logger->info("Storage {} async storing item {} - {}",
                     (void*)storage_.get(),
                     utils::time::to_iso_packed(item->begin),
                     utils::time::to_iso_packed(item->end));

        if (!is_canceled_cb())
            return storage_->store_async(item, finished_cb, is_canceled_cb);

        return true;
    }
};
using timeline_ptr = std::shared_ptr<timeline>;
}  // namespace sync
}  // namespace cloud
}  // namespace vxg
