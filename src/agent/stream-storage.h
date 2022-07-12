#pragma once

#include <agent-proto/objects/config.h>

#include <agent/stream.h>

namespace vxg {
namespace cloud {

class stream_storage : public timed_storage {
    agent::media::stream::ptr stream_;

    bool _clip_to_item(const agent::proto::video_clip_info& clip,
                       item_ptr item) {
        item->begin = clip.tp_start;
        item->end = clip.tp_stop;
        item->data = std::move(clip.data);
        item->state = item::data_state::loaded;
        item->category = agent::proto::command::UC_RECORD;
        item->media_type = agent::proto::command::MT_MP4;
        return item->is_valid();
    }

public:
    using ptr = shared_ptr<stream_storage>;
    stream_storage(agent::media::stream::ptr stream) : stream_ {stream} {}
    virtual ~stream_storage() {}

    virtual std::vector<item_ptr> list(cloud::time start,
                                       cloud::time stop) override {
        auto stream_list = stream_->record_get_list(start, stop, true);
        std::vector<item_ptr> result;

        for (auto& r : stream_list) {
            auto i = std::make_shared<item>();
            if (_clip_to_item(r, i))
                result.push_back(i);
        }

        return result;
    }

    virtual bool load(item_ptr i) override {
        auto clip = stream_->record_export(i->begin, i->end);

        if (clip.tp_start != utils::time::null() &&
            clip.tp_stop != utils::time::null()) {
            return _clip_to_item(clip, i);
        }

        return false;
    }

    virtual bool store(item_ptr) override { return false; }

    virtual bool store_async(
        item_ptr,
        async_store_finished_cb finished_cb,
        async_store_is_canceled_cb is_canceled_cb) override {
        finished_cb(false);
        return false;
    }

    virtual void erase(item_ptr) {}
};
}  // namespace cloud
}  // namespace vxg
