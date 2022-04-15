#ifndef __UNSET_HELPER_H
#define __UNSET_HELPER_H

// FIXME: annotate this file

#include <chrono>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace vxg {
namespace cloud {
namespace time_spec {
using precision = std::chrono::nanoseconds;
template <typename T>
using duration = typename std::conditional<std::is_same<T, precision>::value,
                                           precision,
                                           std::chrono::duration<T> >::type;
}  // namespace time_spec
using time =
    std::chrono::time_point<std::chrono::system_clock, time_spec::precision>;
using duration = time_spec::duration<time_spec::precision>;
}  // namespace cloud
}

inline std::string unset_value_for_impl(std::string*) {
    return std::string("__unset__");
}

//! @brief Returns value of int type that can be treated as unset
//!
//! @return int
//!
inline int unset_value_for_impl(int*) {
    return std::numeric_limits<int>::min();
}

inline double unset_value_for_impl(double*) {
    return std::numeric_limits<double>::min();
}

inline uint64_t unset_value_for_impl(uint64_t*) {
    return std::numeric_limits<uint64_t>::min();
}

inline int64_t unset_value_for_impl(int64_t*) {
    return std::numeric_limits<int64_t>::min();
}

inline vxg::cloud::time unset_value_for_impl(vxg::cloud::time*) {
    using namespace std::chrono;
    return system_clock::time_point(
        duration_cast<microseconds>(duration<double>(0)));
}

inline vxg::cloud::duration unset_value_for_impl(vxg::cloud::duration*) {
    using namespace std::chrono;
    return duration_cast<vxg::cloud::time_spec::precision>(duration<double>(0));
}

inline nlohmann::json unset_value_for_impl(nlohmann::json*) {
    return nullptr;
}

//! @brief Template function which returns object value treated as 'unset' or
//!        uninitialized.
//!
//! @tparam T
//! @return T Value equals to conditionally 'unset'.
//!
template <typename T>
T unset_value_for() {
    return unset_value_for_impl(static_cast<T*>(nullptr));
}

template <typename T>
inline std::vector<T> unset_value_for_impl(std::vector<T>*) {
    if (std::is_same<std::vector<T>, std::string>::value)
        return std::string("__unset__");
    return std::vector<T>(0);
}

template <typename T>
T unset_value_for_impl(T*) {
    if (std::is_same<T, int>::value)
        return std::numeric_limits<int>::min();
    else if (std::is_same<T, double>::value)
        return std::numeric_limits<double>::min();
}

//! @brief Used for objects constructed from json, helps to check if original
//!        json object has specific field.
//!        You need to declare template specification for new types.
//!        @see __is_unset<int>(int t)
//!
//! @tparam T object of type
//! @return true If object's field was actually set during construction, i.e.
//!              original json has such field in it's body.
//! @return false If object's field wasn't set, original json has no such field.
//!               It's also possible that json has such field but its value is
//!               set to value treated as unset value. @see __is_unset<>()
template <typename T>
inline bool __is_unset(T) {
    return false;
}

//! @brief Predicate function checks if int value was not initialized.
//!
//! @tparam int
//! @param t
//! @return true value is uninitalized.
//! @return false value is initialized.
//! @see unset_value_for<int>()
template <>
inline bool __is_unset<int>(int t) {
    return t == unset_value_for<int>();
}

template <>
inline bool __is_unset<std::string>(std::string t) {
    return t == unset_value_for<std::string>();
}

template <>
inline bool __is_unset<double>(double t) {
    return t == unset_value_for<double>();
}

template <>
inline bool __is_unset<vxg::cloud::time>(vxg::cloud::time t) {
    return t == unset_value_for<vxg::cloud::time>();
}

template <>
inline bool __is_unset<vxg::cloud::duration>(vxg::cloud::duration t) {
    return t == unset_value_for<vxg::cloud::duration>();
}

template <>
inline bool __is_unset<nlohmann::json>(nlohmann::json t) {
    // unset if null, empty array or array with null element
    bool is_null = (t == nullptr);
    bool is_empty_array = (t.is_array() && t.empty());

    return (is_null || is_empty_array);
}

template <>
inline bool __is_unset<std::nullptr_t>(std::nullptr_t t) {
    return true;
}

template <typename T>
inline bool __is_unset(nlohmann::json t) {
    // unset if null, empty array or array with null element
    bool is_null = (t == nullptr);
    bool is_empty_array = (t.is_array() && t.empty());

    return (is_null || is_empty_array);
}

//! @brief alternative bool class
//! Standard bool type has two states, this class adds 3rd state - undefined.
//! This class used for json boolean => C++ bool type reflection.
//! The @ref B_INVALID value of the C++ data indicates that source json has no
//! such field.
struct alter_bool {
    //! Internal boolean values
    enum n_alter_bool {
        //! false
        B_FALSE,
        //! true
        B_TRUE,
        //! Undefined, i.e. if the object was constructed from the json object
        //! this value means that original json had no such field.
        B_INVALID = -1
    };

    alter_bool(const n_alter_bool& v) { val = v; }

    alter_bool(const bool& v) {
        if (v)
            val = B_TRUE;
        else
            val = B_FALSE;
    }

    alter_bool operator=(const bool& b) {
        if (b)
            return alter_bool(val = B_TRUE);
        else
            return alter_bool(val = B_FALSE);
    }

    operator bool() const {
        if (val == B_TRUE)
            return true;
        return false;
    }

    friend void from_json(const nlohmann::json& j, alter_bool& c) {
        if (j.is_boolean())
            c.val = j == true ? B_TRUE : B_FALSE;
        else
            c.val = B_INVALID;
    }

    friend void to_json(nlohmann::json& j, const alter_bool& c) {
        if (c.val == B_TRUE)
            j = true;
        else
            j = false;
    }

    n_alter_bool val {B_INVALID};
};
template <>
inline bool __is_unset<alter_bool>(alter_bool t) {
    return t.val == alter_bool::B_INVALID;
}

const std::string UnsetString = unset_value_for<std::string>();
const vxg::cloud::time UnsetTime = unset_value_for<vxg::cloud::time>();
const vxg::cloud::duration UnsetDuration =
    unset_value_for<vxg::cloud::duration>();
const int UnsetInt = unset_value_for<int>();
const double UnsetFloat = unset_value_for<double>();
const double UnsetDouble = unset_value_for<double>();
const uint64_t UnsetUInt64 = unset_value_for<uint64_t>();
const int64_t UnsetInt64 = unset_value_for<int64_t>();

#endif