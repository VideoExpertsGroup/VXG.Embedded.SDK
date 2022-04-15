#pragma once

#include <fstream>

#include <utils/logging.h>
#include <nlohmann/json.hpp>

namespace vxg {
//! @brief Simple properties key-value file storage class
//!        Class privides simple kv setter/getter interface for the JSON
//!        dictionary storred in the file.
//!        File with defaults used to initialize property during the get()
//!        if its value wasn't set() yet.
class properties {
public:
    static std::string props_filename_;
    static std::string default_props_filename_;
    static std::mutex lock_;
    static nlohmann::json default_json_;

private:
    /**
     * @brief Create props file with default settings
     *
     * @return true file create
     * @return false file not created
     */
    static void __props_file_reset() {
        nlohmann::json j;

        vxg::logger::info("Reseting props file {}", props_filename_);

        __load(default_props_filename_, j);
        __store(props_filename_, j);
    }

    static bool __load(std::string filepath,
                       nlohmann::json& j,
                       bool merge = false) {
        if (!merge || j == nullptr)
            j = {};
        try {
            std::ifstream f(filepath);
            if (!f.fail()) {
                try {
                    f >> j;
                } catch (...) {
                    if (filepath != default_props_filename_)
                        __load(default_props_filename_, j);
                    else
                        j = {};
                }
                f.close();
            }
        } catch (std::exception& e) {
            vxg::logger::error("Unable to load {} : {}", filepath, e.what());
        }
        return true;
    }

    static bool __store(std::string filepath, nlohmann::json j) {
        try {
            std::ofstream f(filepath);
            if (!f.fail()) {
                f << j;
                f.close();
                return true;
            }
        } catch (std::exception& e) {
            vxg::logger::error("Unable to save {} : {}", filepath, e.what());
        }
        return false;
    }

    static bool __set(std::string key,
                      std::string val,
                      std::string props_file = props_filename_) {
        nlohmann::json j;
        if (__load(props_file, j)) {
            j[key] = val;
            return __store(props_file, j);
        } else {
            vxg::logger::error("Unable to open props file: {}",
                               strerror(errno));
        }
        return false;
    }

    static bool __get(const std::string& key,
                      std::string& val,
                      std::string props_file = props_filename_) {
        nlohmann::json j;
        if (__load(props_file, j)) {
            if (j.contains(key))
                val = j[key].get<std::string>();
            else
                val = "";
            return true;
        } else {
            vxg::logger::error("Unable to open props file: {}",
                               strerror(errno));
        }
        return false;
    }

    static void _apply_defaults_if_needed(bool force) {
        using namespace nlohmann;
        nlohmann::json j;
        try {
            if (__load(default_props_filename_, j)) {
                for (auto it : j.items()) {
                    std::string key = it.key();
                    std::string val;
                    std::string default_val;

                    __get(key, val, props_filename_);
                    __get(key, default_val, default_props_filename_);

                    if (force)
                        __set(key, default_val);
                    else if (!j.contains(key))
                        __set(key, default_val);
                }
            } else {
                vxg::logger::error("Failed to open default props file {}",
                                   default_props_filename_);
            }
        } catch (const std::exception& e) {
            vxg::logger::error("Failed process default props file {}: {}",
                               default_props_filename_, e.what());
        }
    }

public:
    static void set_props_filepath(std::string file) { props_filename_ = file; }
    static void set_default_props_filepath(std::string file) {
        default_props_filename_ = file;
        bool force = false;
        std::string ver, default_ver;
        __get("version", ver, props_filename_);
        __get("version", default_ver, default_props_filename_);

        if (ver != default_ver) {
            vxg::logger::info(
                "Default props version {} != current props version {}", ver,
                default_ver);
            vxg::logger::info("Reseting to new defaults");

            // Force rewriting all keys presented in default config
            force = true;
        }

        _apply_defaults_if_needed(force);
    }
    static std::string get_props_filepath() { return props_filename_; }
    static std::string get_default_props_filepath() {
        return default_props_filename_;
    }

    static void set(std::string key, std::string val) {
        std::lock_guard<std::mutex> guard(lock_);

        if (!__set(key, val)) {
            _apply_defaults_if_needed(false);
            if (!__set(key, val)) {
                vxg::logger::error("Not possible to set property, failing");
            }
        }
    }

    static std::string get(std::string key) {
        std::lock_guard<std::mutex> guard(lock_);
        std::string value = "";

        if (!__get(key, value)) {
            vxg::logger::error("Not possible to get property, failing");
        }

        return value;
    }

    static bool reset(std::string props_filepath,
                      std::string default_props_filepath = "") {
        nlohmann::json j;

        set_props_filepath(props_filepath);

        if (!default_props_filepath.empty())
            set_default_props_filepath(default_props_filepath);
        else
            set_default_props_filepath(props_filepath + ".default");

        __load(props_filepath, j);
        if (!__store(props_filepath, j)) {
            vxg::logger::error("Unable to open props file {} for write",
                               props_filepath);
            return false;
        }

        if (!__load(default_props_filepath, j)) {
            vxg::logger::error("Unable to open default props file {} for write",
                               default_props_filepath);
            return false;
        }

        return true;
    }
};
}  // namespace vxg