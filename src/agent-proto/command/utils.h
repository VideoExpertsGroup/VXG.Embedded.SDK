#ifndef __COMMAND_UTILS_H
#define __COMMAND_UTILS_H

#include <nlohmann/json.hpp>

#include <agent-proto/command/unset-helper.h>

// namespace CameraManagerProtocol {
// namespace utils {

// #define __is_unset(v1) (std::is_same<int, typeof(v1)>::value && v1 ==
// (CameraManagerProtocol::utils::unset_value_for<typeof(v1)>()))

#ifndef ignore_exception
#define ignore_exception(...) \
    try {                     \
        __VA_ARGS__;          \
    } catch (...) {           \
    }
#endif

// #undef NLOHMANN_JSON_TO
#define MY_JSON_TO(v1)                                 \
    {                                                  \
        if (!__is_unset(nlohmann_json_t.v1)) {         \
            nlohmann_json_j[#v1] = nlohmann_json_t.v1; \
            if (nlohmann_json_j[#v1] == nullptr)       \
                nlohmann_json_j.erase(#v1);            \
        }                                              \
    }
// #undef NLOHMANN_JSON_FROM
#define MY_JSON_FROM(v1) \
    ignore_exception(nlohmann_json_j.at(#v1).get_to(nlohmann_json_t.v1));

#define JSON_DEFINE_TYPE_INTRUSIVE(Type, ...)                                \
    friend void to_json(nlohmann::json& nlohmann_json_j,                     \
                        const Type& nlohmann_json_t) {                       \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(MY_JSON_TO, __VA_ARGS__))   \
    }                                                                        \
    friend void from_json(const nlohmann::json& nlohmann_json_j,             \
                          Type& nlohmann_json_t) {                           \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(MY_JSON_FROM, __VA_ARGS__)) \
    }

#define JSON_DEFINE_DERIVED_TYPE_INTRUSIVE(Type, BaseType, ...)              \
    friend void to_json(nlohmann::json& nlohmann_json_j,                     \
                        const Type& nlohmann_json_t) {                       \
        to_json(nlohmann_json_j, static_cast<BaseType>(nlohmann_json_t));    \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(MY_JSON_TO, __VA_ARGS__))   \
    }                                                                        \
    friend void from_json(const nlohmann::json& nlohmann_json_j,             \
                          Type& nlohmann_json_t) {                           \
        from_json(nlohmann_json_j, static_cast<BaseType&>(nlohmann_json_t)); \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(MY_JSON_FROM, __VA_ARGS__)) \
    }

#define JSON_DEFINE_DERIVED_TYPE_INTRUSIVE_NO_ARGS(Type, BaseType)           \
    friend void to_json(nlohmann::json& nlohmann_json_j,                     \
                        const Type& nlohmann_json_t) {                       \
        to_json(nlohmann_json_j, static_cast<BaseType>(nlohmann_json_t));    \
    }                                                                        \
    friend void from_json(const nlohmann::json& nlohmann_json_j,             \
                          Type& nlohmann_json_t) {                           \
        from_json(nlohmann_json_j, static_cast<BaseType&>(nlohmann_json_t)); \
    }

#define JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE(Type, BaseType1, BaseType2, ...)  \
    friend void to_json(nlohmann::json& nlohmann_json_j,                      \
                        const Type& nlohmann_json_t) {                        \
        to_json(nlohmann_json_j, static_cast<BaseType1>(nlohmann_json_t));    \
        to_json(nlohmann_json_j, static_cast<BaseType2>(nlohmann_json_t));    \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(MY_JSON_TO, __VA_ARGS__))    \
    }                                                                         \
    friend void from_json(const nlohmann::json& nlohmann_json_j,              \
                          Type& nlohmann_json_t) {                            \
        from_json(nlohmann_json_j, static_cast<BaseType1&>(nlohmann_json_t)); \
        from_json(nlohmann_json_j, static_cast<BaseType2&>(nlohmann_json_t)); \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(MY_JSON_FROM, __VA_ARGS__))  \
    }

#define JSON_DEFINE_DERIVED2_TYPE_INTRUSIVE_NO_ARGS(Type, BaseType1,          \
                                                    BaseType2)                \
    friend void to_json(nlohmann::json& nlohmann_json_j,                      \
                        const Type& nlohmann_json_t) {                        \
        to_json(nlohmann_json_j, static_cast<BaseType1>(nlohmann_json_t));    \
        to_json(nlohmann_json_j, static_cast<BaseType2>(nlohmann_json_t));    \
    }                                                                         \
    friend void from_json(const nlohmann::json& nlohmann_json_j,              \
                          Type& nlohmann_json_t) {                            \
        from_json(nlohmann_json_j, static_cast<BaseType1&>(nlohmann_json_t)); \
        from_json(nlohmann_json_j, static_cast<BaseType2&>(nlohmann_json_t)); \
    }
// }  // namespace utils
// }  // namespace CameraManagerProtocol

#endif