#ifndef __FFMPEG_SEGMENTED_SINK_H
#define __FFMPEG_SEGMENTED_SINK_H

#include <functional>
#include "ffmpeg_sink.h"

namespace vxg {
namespace media {
namespace ffmpeg {
class segmented_sink : public ffmpeg::Sink {
    vxg::logger::logger_ptr logger {
        vxg::logger::instance("ffmpeg-multifile-sink")};

public:
    segmented_sink(
        std::chrono::seconds segment_duration = std::chrono::seconds(30),
        std::string format = "mp4")
        : ffmpeg::Sink(), format_ {format} {}
    virtual ~segmented_sink() {}
    virtual std::string name() { return "ffmpeg-segmented-sink"; }

    virtual bool init(std::string storage_path) {
        // "/out%Y-%m-%d_%H-%M-%S.%M.mp4";
        std::string filepath_templ = storage_path + "%d.mp4";
        std::string segment_list_filepath = storage_path + "segments_list.csv";
        AVDictionary* opts(0);
        std::string time_offset = std::to_string(
            std::chrono::duration<double>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());

        storage_path_ = storage_path;

        av_dict_set(&opts, "segment_format", format_.c_str(), 0);
        av_dict_set(&opts, "segment_list_type", "csv", 0);
        av_dict_set(&opts, "segment_list", segment_list_filepath.c_str(), 0);
        av_dict_set_int(&opts, "segment_list_size", 10, 0);
        // av_dict_set_int(&opts, "segment_clocktime_wrap_duration",
        //                 segment_duration_.count(), 0);
        av_dict_set_int(&opts, "segment_time", segment_duration_.count(), 0);
        av_dict_set_int(&opts, "segment_wrap", 10, 0);
        av_dict_set(&opts, "output_ts_offset", time_offset.c_str(), 0);
        av_dict_set(&opts, "segment_list_flags", "cache+live", 0);
        av_dict_set(&opts, "segment_save_cur_idx", "1", 0);

        ffmpeg::Sink::set_ff_opts(opts);
        return ffmpeg::Sink::init(filepath_templ, "segment");
    }

    std::string storage_path() { return storage_path_; }

    // virtual bool init(std::string uri) override {
    //     return ffmpeg::Sink::init(uri, format_);
    // }

protected:
    std::string format_;
    std::string storage_path_ {"/tmp/"};
    std::chrono::seconds segment_duration_ {10};
};  // class Sink
}  // namespace ffmpeg
}  // namespace media
}  // namespace vxg

#endif