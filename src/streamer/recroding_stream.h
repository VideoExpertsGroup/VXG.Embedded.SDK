#ifndef __RECORDING_STREAM_H
#define __RECORDING_STREAM_H

#include <streamer/ffmpeg_multifile_sink.h>
#include <streamer/ffmpeg_segmented_sink.h>
#include <streamer/rtsp_source.h>
#include <streamer/stream.h>
#include <utils/logging.h>
#include <utils/utils.h>

#include <experimental/filesystem>

namespace vxg {
namespace media {

class mp4_recorder_sink : public vxg::media::ffmpeg::multifile_sink {
    vxg::logger::logger_ptr logger {vxg::logger::instance(name())};
    typedef std::function<void(
        std::string,
        std::chrono::time_point<std::chrono::system_clock> begin,
        std::chrono::milliseconds duration)>
        on_new_recording_func;

    on_new_recording_func on_new_recroding_cb_ {nullptr};

    std::string storage_path_;
    std::chrono::seconds storage_retention_time_ {std::chrono::minutes(15)};
    bool started_ {false};
    std::chrono::seconds segment_duration_;

    struct recording {
        std::experimental::filesystem::path path;
        vxg::cloud::time begin;
        vxg::cloud::time end;

        recording(std::experimental::filesystem::path _path,
                  vxg::cloud::time _begin = vxg::cloud::utils::time::null(),
                  vxg::cloud::time _end = vxg::cloud::utils::time::null())
            : path(_path), begin(_begin), end(_end) {}

        bool operator<(const recording& rv) { return (begin < rv.begin); }
    };
    std::vector<recording> recordings_;
    std::mutex recordings_lock_;

    bool _get_period_from_filename(std::string filename,
                                   vxg::cloud::time& begin,
                                   vxg::cloud::time& end) {
        using namespace vxg::cloud;
        auto timestamps = utils::string_split(filename, '_');

        begin = end = utils::time::null();

        if (timestamps.size() == 2) {
            try {
                begin = utils::time::from_double(std::stod(timestamps[0]));
                end = utils::time::from_double(std::stod(timestamps[1]));
            } catch (...) {
                // Don't throw exception if can't parse the timestamps in the
                // filename
            }
        }

        if (begin == utils::time::null() || end == utils::time::null()) {
            logger->warn("Can't parse timestamps from '{}'", filename);
            return false;
        }

        return true;
    }

    void _rotate_recordings() {
        using namespace std::chrono;
        using namespace std::experimental::filesystem;
        const std::lock_guard<std::mutex> lock(recordings_lock_);
        auto now = system_clock::now();

        for (auto it = recordings_.begin(); it != recordings_.end();) {
            if (now - it->end > storage_retention_time_) {
                std::string filename = it->path.filename();
                logger->debug("Cleanup old recording '{}'", filename);

                // Delete file
                remove(it->path);

                // Erase item from list
                it = recordings_.erase(it);
            } else
                it++;
        }
    }

    bool _add_recording(recording r) {
        {
            const std::lock_guard<std::mutex> lock(recordings_lock_);

            if (r.end - r.begin > segment_duration_ * 2)
                return false;

            logger->debug("Add recording '{}' to recordings list",
                          (const char*)r.path.filename().c_str());

            recordings_.push_back(r);
        }

        _rotate_recordings();

        return true;
    }

    bool _load_recordings() {
        using namespace std::experimental::filesystem;
        using namespace vxg::cloud;
        using namespace std::chrono;
        for (auto& p : directory_iterator(storage_path_)) {
            std::string filename(p.path().filename());
            recording r(p.path());

            // If filename format is valid add to recordings
            if (_get_period_from_filename(filename, r.begin, r.end) &&
                (r.end - r.begin < segment_duration_ * 2)) {
                _add_recording(r);
                // If filename format is invalid but filename ends with .part
                // delete
            } else if (utils::string_endswith(p.path().filename(), ".part")) {
                remove(p);
                continue;
            }
        }
        return true;
    }

    void _on_new_recroding(
        std::string filepath,
        std::chrono::time_point<std::chrono::system_clock> begin,
        std::chrono::milliseconds duration) {
        using namespace std::experimental::filesystem;
        using namespace vxg::cloud::utils::time;
        std::string final_filepath =
            path(storage_path_) /
            path(to_iso_packed(begin) + "_" + to_iso_packed(begin + duration));

        if (duration > fragment_duration() * 2) {
            logger->error("Fragment too big, possible a bug, removing!");
            remove(filepath);
            return;
        }

        try {
            rename(filepath, final_filepath);

            if (exists(final_filepath)) {
                logger->info("New recording {}", final_filepath);

                _add_recording(
                    recording(final_filepath, begin, begin + duration));
            } else {
                logger->error("Recording {} finalization failed", filepath);
            }
        } catch (std::exception& e) {
            logger->warn("Failed to finalize {} {}", filepath, e.what());
        }
    }

public:
    mp4_recorder_sink(
        std::chrono::seconds segment_duration = std::chrono::seconds(10),
        on_new_recording_func on_new_recroding_cb = nullptr /* not used */)
        : multifile_sink(MultiFrag::Sink::DURATION,
                         segment_duration.count(),
                         "mov"),
          on_new_recroding_cb_ {on_new_recroding_cb},
          segment_duration_ {segment_duration} {}

    virtual bool init(std::string storage_dir) override {
        if (!std::experimental::filesystem::is_directory(storage_dir)) {
            logger->error("Recordings storage path {} is not a directory",
                          storage_dir);
            return false;
        }
        storage_path_ = storage_dir;

        _load_recordings();

        return multifile_sink::init(storage_path_);
    }

    std::vector<recording> export_recordings(vxg::cloud::time begin,
                                             vxg::cloud::time end) {
        const std::lock_guard<std::mutex> lock(recordings_lock_);
        std::vector<recording> result;

        for (auto& r : recordings_) {
            // Records time intersection
            // Condition constructed as NOT non-intersection
            if (begin < r.end && end > r.begin)
                result.push_back(r);
        }

        return result;
    }

protected:
    virtual std::string fragment_generate_uri(size_t id) override {
        using namespace std::experimental::filesystem;
        using namespace vxg::cloud::utils::time;

        std::string filename = to_iso_packed(now()) + ".part";
        path rpath = path(multifile_sink::storage_path()) / filename;

        logger->debug("New fragment '{}' recording started", filename);

        return rpath;
    }

    virtual void fragment_finalized(
        std::string filename,
        std::chrono::time_point<std::chrono::system_clock> begin,
        std::chrono::milliseconds duration) override {
        using namespace vxg::cloud;

        logger->info("Fragment '{}' of {} ms finalized.", filename,
                     duration.count());

        _on_new_recroding(filename, begin, duration);
    }

    virtual std::string name() override { return "mp4-recorder-sink"; }
};

// class recording_stream : public vxg::media::stream {
//     vxg::logger::logger_ptr logger {
//         vxg::logger::instance("media-recording-stream")};

// public:
//     typedef std::shared_ptr<recording_stream> ptr;

//     recording_stream(std::string name,
//                      vxg::media::Streamer::ISource::ptr source)
//         : vxg::media::stream(name,
//                              source,
//                              std::make_shared<vxg::media::MultiFrag::Sink>())
//                              {}

//     virtual ~recording_stream() {}
// };

class rtsp_mp4_recording_stream : public vxg::media::stream {
    vxg::logger::logger_ptr logger {
        vxg::logger::instance("mp4-recording-stream")};
    std::string storage_path_;
    std::chrono::seconds storage_retention_time_ {std::chrono::hours(2)};
    bool started_ {false};

    struct recording {
        std::experimental::filesystem::path path;
        vxg::cloud::time begin;
        vxg::cloud::time end;

        recording(std::experimental::filesystem::path _path,
                  vxg::cloud::time _begin = vxg::cloud::utils::time::null(),
                  vxg::cloud::time _end = vxg::cloud::utils::time::null())
            : path(_path), begin(_begin), end(_end) {}

        bool operator<(const recording& rv) { return (begin < rv.begin); }
    };
    std::vector<recording> recordings_;
    std::mutex recordings_lock_;

    static std::vector<vxg::media::Streamer::MediaType> _rtsp_medias_filter() {
        std::vector<vxg::media::Streamer::MediaType> medias;
        medias.push_back(vxg::media::Streamer::VIDEO);
        // medias.push_back(vxg::media::Streamer::AUDIO);
        return medias;
    }

    bool _get_period_from_filename(std::string filename,
                                   vxg::cloud::time& begin,
                                   vxg::cloud::time& end) {
        using namespace vxg::cloud;
        auto timestamps = utils::string_split(filename, '_');

        begin = end = utils::time::null();

        if (timestamps.size() == 2) {
            begin = utils::time::from_double(std::stod(timestamps[0]));
            end = utils::time::from_double(std::stod(timestamps[1]));
        }

        if (begin == utils::time::null() || end == utils::time::null()) {
            logger->warn("Can't parse timestamps from '{}'", filename);
            return false;
        }

        return true;
    }

    void _rotate_recordings() {
        using namespace std::chrono;
        using namespace std::experimental::filesystem;
        const std::lock_guard<std::mutex> lock(recordings_lock_);
        auto now = system_clock::now();

        for (auto it = recordings_.begin(); it != recordings_.end();) {
            if (now - it->end > storage_retention_time_) {
                std::string filename = it->path.filename();
                logger->debug("Cleanup old recording '{}'", filename);

                // Delete file
                remove(it->path);

                // Erase item from list
                it = recordings_.erase(it);
            } else
                it++;
        }
    }

    bool _add_recording(recording r) {
        {
            const std::lock_guard<std::mutex> lock(recordings_lock_);

            logger->info("Add recording '{}' to recordings list",
                         (const char*)r.path.filename().c_str());

            recordings_.push_back(r);
        }

        _rotate_recordings();

        return true;
    }

    bool _load_recordings() {
        using namespace std::experimental::filesystem;
        using namespace vxg::cloud;
        for (auto& p : directory_iterator(storage_path_)) {
            std::string filename(p.path().filename());
            recording r(p.path());

            // If filename format is valid add to recordings
            if (_get_period_from_filename(filename, r.begin, r.end)) {
                _add_recording(r);
                // If filename format is invalid but filename ends with .part
                // delete
            } else if (utils::string_endswith(p.path().filename(), ".part")) {
                remove(p);
                continue;
            }
        }
        return true;
    }

    void _on_new_recroding(
        std::string filepath,
        std::chrono::time_point<std::chrono::system_clock> begin,
        std::chrono::milliseconds duration) {
        using namespace std::experimental::filesystem;
        using namespace vxg::cloud::utils::time;
        std::string final_filepath =
            path(storage_path_) /
            path(to_iso_packed(begin) + "_" + to_iso_packed(begin + duration));

        rename(filepath, final_filepath);

        if (exists(final_filepath)) {
            logger->info("New recording {}", filepath);

            _add_recording(recording(final_filepath, begin, begin + duration));
        } else {
            logger->error("Recording {} finalization failed", filepath);
        }
    }

    //! @private
    //! @brief Check source pointer, if it was null i.e. user didn't specify it,
    //! this function creates default source, otherwise just pass argument.
    //!
    //! @return Streamer::ISource::ptr
    //!
    static Streamer::ISource::ptr _check_source(Streamer::ISource::ptr source) {
        if (!source)
            return std::make_shared<vxg::media::rtsp_source>(
                true, _rtsp_medias_filter());
        return source;
    }

    void _dump_recordings() {
        for (auto& r : recordings_) {
            std::string file = r.path.filename();
            logger->info("{}", file);
        }
    }

public:
    typedef std::shared_ptr<rtsp_mp4_recording_stream> ptr;

    rtsp_mp4_recording_stream(Streamer::ISource::ptr source = nullptr)
        : vxg::media::stream(
              "mp4-recording",
              _check_source(source),
              std::make_shared<mp4_recorder_sink>(
                  std::chrono::seconds(10),
                  std::bind(&rtsp_mp4_recording_stream::_on_new_recroding,
                            this,
                            std::placeholders::_1,
                            std::placeholders::_2,
                            std::placeholders::_3))) {}

    bool start(std::string rtsp_url, std::string storage_dir) {
        if (started_) {
            logger->warn("Trying to start already started recorder!");
            return true;
        }

        if (!std::experimental::filesystem::is_directory(storage_dir)) {
            logger->error("Recordings storage path {} is not a directory",
                          storage_dir);
            return false;
        }
        storage_path_ = storage_dir;

        _load_recordings();

        started_ = stream::start(rtsp_url) && stream::start_sink(storage_dir);

        if (!started_) {
            stream::stop_sink();
            stream::stop();
        }

        return started_;
    }

    virtual void stop() override {
        if (started_) {
            stream::stop_sink();
            stream::stop();
        }
        started_ = false;
    }

    bool is_running() { return started_; }

    std::vector<recording> export_recordings(vxg::cloud::time begin,
                                             vxg::cloud::time end) {
        using namespace vxg::cloud;
        const std::lock_guard<std::mutex> lock(recordings_lock_);
        std::vector<recording> result;

        for (auto& r : recordings_) {
            // If requested begin or end lays inside of the recording this
            // recording should be exported
            if ((begin >= r.begin && begin < r.end) ||
                (end > r.begin && end <= r.end))
                result.push_back(r);
        }

        if (result.empty()) {
            logger->warn("Can't export recording for {} - {}",
                         utils::time::to_iso(begin), utils::time::to_iso(end));
            logger->info("Current records list:");
            _dump_recordings();
        }

        return result;
    }
};

class rtsp_segmented_mp4_recording_stream : public vxg::media::stream {
    vxg::logger::logger_ptr logger {
        vxg::logger::instance("segmented-mp4-recording-stream")};
    std::string storage_path_;
    std::chrono::seconds storage_retention_time_ {std::chrono::minutes(15)};
    bool started_ {false};

    struct recording {
        std::experimental::filesystem::path path;
        vxg::cloud::time begin;
        vxg::cloud::time end;

        recording(std::experimental::filesystem::path _path,
                  vxg::cloud::time _begin = vxg::cloud::utils::time::null(),
                  vxg::cloud::time _end = vxg::cloud::utils::time::null())
            : path(_path), begin(_begin), end(_end) {}

        bool operator<(const recording& rv) { return (begin < rv.begin); }
    };
    std::vector<recording> recordings_;
    std::mutex recordings_lock_;

    static std::vector<vxg::media::Streamer::MediaType> _rtsp_medias_filter() {
        std::vector<vxg::media::Streamer::MediaType> medias;
        medias.push_back(vxg::media::Streamer::VIDEO);
        medias.push_back(vxg::media::Streamer::AUDIO);
        return medias;
    }

    bool _get_period_from_filename(std::string filename,
                                   vxg::cloud::time& begin,
                                   vxg::cloud::time& end) {
        using namespace vxg::cloud;
        auto timestamps = utils::string_split(filename, '_');

        begin = end = utils::time::null();

        if (timestamps.size() == 2) {
            begin = utils::time::from_double(std::stod(timestamps[0]));
            end = utils::time::from_double(std::stod(timestamps[1]));
        }

        if (begin == utils::time::null() || end == utils::time::null()) {
            logger->warn("Can't parse timestamps from '{}'", filename);
            return false;
        }

        return true;
    }

    void _rotate_recordings() {
        using namespace std::chrono;
        using namespace std::experimental::filesystem;
        const std::lock_guard<std::mutex> lock(recordings_lock_);
        auto now = system_clock::now();

        for (auto it = recordings_.begin(); it != recordings_.end();) {
            if (now - it->end > storage_retention_time_) {
                std::string filename = it->path.filename();
                logger->debug("Cleanup old recording '{}'", filename);

                // Delete file
                remove(it->path);

                // Erase item from list
                it = recordings_.erase(it);
            } else
                it++;
        }
    }

    bool _add_recording(recording r) {
        {
            const std::lock_guard<std::mutex> lock(recordings_lock_);

            logger->debug("Add recording '{}' to recordings list",
                          (const char*)r.path.filename().c_str());

            recordings_.push_back(r);
        }

        _rotate_recordings();

        return true;
    }

    bool _load_recordings() {
        using namespace std::experimental::filesystem;
        using namespace vxg::cloud;
        for (auto& p : directory_iterator(storage_path_)) {
            std::string filename(p.path().filename());
            recording r(p.path());

            // If filename format is valid add to recordings
            if (_get_period_from_filename(filename, r.begin, r.end)) {
                _add_recording(r);
                // If filename format is invalid but filename ends with .part
                // delete
            } else if (utils::string_endswith(p.path().filename(), ".part")) {
                remove(p);
                continue;
            }
        }
        return true;
    }

    void _on_new_recroding(
        std::string filepath,
        std::chrono::time_point<std::chrono::system_clock> begin,
        std::chrono::milliseconds duration) {
        using namespace std::experimental::filesystem;
        using namespace vxg::cloud::utils::time;
        std::string final_filepath =
            path(storage_path_) /
            path(to_iso_packed(begin) + "_" + to_iso_packed(begin + duration));

        rename(filepath, final_filepath);

        if (exists(final_filepath)) {
            logger->info("New recording {}", filepath);

            _add_recording(recording(final_filepath, begin, begin + duration));
        } else {
            logger->error("Recording {} finalization failed", filepath);
        }
    }

public:
    rtsp_segmented_mp4_recording_stream()
        : vxg::media::stream(
              "mp4-segmented-recording",
              std::make_shared<vxg::media::rtsp_source>(true,
                                                        _rtsp_medias_filter()),
              std::make_shared<ffmpeg::segmented_sink>(
                  std::chrono::seconds(5))) {}

    bool start(std::string rtsp_url, std::string storage_dir) {
        if (!std::experimental::filesystem::is_directory(storage_dir)) {
            logger->error("Recordings storage path {} is not a directory",
                          storage_dir);
            return false;
        }
        storage_path_ = storage_dir;

        _load_recordings();

        started_ = stream::start(rtsp_url) && stream::start_sink(storage_dir);

        if (!started_) {
            stream::stop_sink();
            stream::stop();
        }

        return started_;
    }

    virtual void stop() override {
        if (started_) {
            stream::stop_sink();
            stream::stop();
        }
        started_ = false;
    }

    bool is_running() { return started_; }

    std::vector<recording> export_recordings(vxg::cloud::time begin,
                                             vxg::cloud::time end) {
        const std::lock_guard<std::mutex> lock(recordings_lock_);
        std::vector<recording> result;

        for (auto& r : recordings_) {
            // If requested begin or end lays inside of the recording this
            // recording should be exported
            if ((begin >= r.begin && begin < r.end) ||
                (end > r.begin && end <= r.end))
                result.push_back(r);
        }

        return result;
    }
};

}  // namespace media
}  // namespace vxg
#endif