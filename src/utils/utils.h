#ifndef __UTILS_
#define __UTILS_

#include <chrono>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>

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

namespace utils {

void set_thread_name(std::string name);

namespace time {

inline cloud::time now() {
    return std::chrono::time_point_cast<time_spec::precision>(
        std::chrono::system_clock::now());
}

std::string time_to_ISO8601(std::time_t);
std::string time_to_ISO8601_packed(std::time_t);
std::string now_ISO8601_UTC();
std::string now_ISO8601_UTC_packed();
std::time_t now_time_UTC();
std::time_t ISO8601_to_time(const std::string& input);
std::string to_iso_8601(cloud::time t);

std::string to_iso(cloud::time t);
std::string to_iso2(cloud::time t);
std::string to_iso_packed(cloud::time t);
std::string to_iso_local(cloud::time t);
cloud::time from_double(double t);
double to_double(cloud::time t);
cloud::time from_iso(std::string st);
cloud::time from_iso2(std::string st);
cloud::time from_iso_packed(std::string st);
bool iso_time_valid(const std::string& s);
inline cloud::time null() {
    return from_double(0);
};

inline cloud::time max() {
    return cloud::time::max();
}

bool is_iso_packed(const std::string& s);
bool is_iso(const std::string& s);

};  // namespace time

struct uri {
    std::string scheme;
    std::string user;
    std::string password;
    std::string host;
    std::string port;
    std::string path;
    std::string query;
    std::string fragment;

    static bool parse(const std::string& in_uri, uri& result) {
        // match http or https before the ://
        const char* SCHEME_REGEX = "(([A-Za-z]+?)://)?";
        // match anything other than @ / : or whitespace before the ending @
        const char* USER_REGEX = "(([^@/:]+):)?";
        const char* PASSWORD_REGEX = "(([^/:]+)@)?";
        // mandatory. match anything other than @ / : or whitespace
        const char* HOST_REGEX = "([^@/:]+)";
        // after the : match 1 to 5 digits
        const char* PORT_REGEX = "(:([0-9]{1,5}))?";
        // after the / match anything other than : # ? or whitespace
        const char* PATH_REGEX = "(/[^:#?]*)?";
        // after the ? match any number of x=y pairs, seperated by & or ;
        // const char* QUERY_REGEX =
        // "(\?(([^?;&#=]+=[^?;&#=]+)([;|&]([^?;&#=]+=[^?;&#=]+))*))?";
        // after the # match anything other than # or whitespace
        // const char* FRAGMENT_REGEX = "(#([^#]*))?";

        auto regex_str = std::string("^") + SCHEME_REGEX + USER_REGEX +
                         PASSWORD_REGEX + HOST_REGEX + PORT_REGEX + PATH_REGEX +
                         // QUERY_REGEX +
                         // FRAGMENT_REGEX
                         "$";
        static const std::regex regExpr(regex_str);

        std::smatch matchResults;
        if (std::regex_match(in_uri.cbegin(), in_uri.cend(), matchResults,
                             regExpr)) {
            result.scheme.assign(matchResults[2].first, matchResults[2].second);
            result.user.assign(matchResults[4].first, matchResults[4].second);
            result.password.assign(matchResults[6].first,
                                   matchResults[6].second);

            result.host.assign(matchResults[7].first, matchResults[7].second);
            result.port.assign(matchResults[9].first, matchResults[9].second);
            result.path.assign(matchResults[10].first, matchResults[10].second);
            result.query.assign(matchResults[12].first,
                                matchResults[12].second);
            result.fragment.assign(matchResults[16].first,
                                   matchResults[16].second);

            return true;
        }

        return false;
    }
};

namespace motion {

struct map : public std::string {
    explicit map() {}

    map(const map& motionMap) { this->assign(motionMap); }

    map& operator=(const std::string& motionMap) {
        this->assign(motionMap);

        return *this;
    }

    static std::string pack(const std::string& unpackedGrid);
    static std::string unpack(const std::string& packedMap, size_t outputLen);
};  // struct map

};  // namespace motion

// disable format security warning here
// function, intruder is not able to construct bad-formaed format as we are the
// only ones who construct formats
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
template <typename... Args>
std::string string_format(const std::string& format, Args... args) {
    size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
    if (size <= 0) {
        throw std::runtime_error("Error during formatting.");
    }
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1);
}
#pragma GCC diagnostic pop

std::string string_trim(const std::string& name, std::regex regx);
std::string string_trim(const std::string& name);
std::vector<std::string> string_split(const std::string& s, char delimiter);
bool string_startswith(std::string const& fullString, std::string const& start);
bool string_endswith(std::string const& fullString, std::string const& ending);
bool string_replace(std::string& str,
                    const std::string& from,
                    const std::string& to);
std::string string_urlencode(const std::string& value);
std::string string_urldecode(const std::string& text);
std::string string_tolower(const std::string& s);
std::string string_toupper(const std::string& s);
inline bool string_contains(std::string s, char c) {
    return s.find(c) != std::string::npos;
}
std::string dirname(const std::string& filepath);

namespace gcc_abi {
std::string demangle(std::string name);
}  // namespace gcc_abi

}  // namespace utils
}  // namespace cloud
}  // namespace vxg

// C++ backports
namespace std {
#if __cplusplus < 201703L  // C++17
#if __cplusplus < 201402L  // C++14

// std::make_unique for C++11
template <typename T, typename... CONSTRUCTOR_ARGS>
std::unique_ptr<T> make_unique(CONSTRUCTOR_ARGS&&... constructor_args) {
    return std::unique_ptr<T>(
        new T(std::forward<CONSTRUCTOR_ARGS>(constructor_args)...));
}
#endif  // __cplusplus == 201402L
#endif  // __cplusplus == 201703L
}  // namespace std
#endif