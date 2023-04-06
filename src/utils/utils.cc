#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <map>
#include <memory>
#include <regex>
#include <sstream>

#include <date/tz.h>

#include <utils/base64.h>
#include <utils/logging.h>
#include <utils/packbits.h>
#include <utils/utils.h>

namespace vxg {
namespace cloud {
namespace utils {

void set_thread_name(std::string name) {
#ifdef __APPLE__
    pthread_setname_np(name.c_str());
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
    pthread_set_name_np(pthread_self(), name.c_str());
#elif defined(__linux__) || defined(__sun)
    char thread_name[16] = {0};
    strncpy(thread_name, name.c_str(), 15);
    pthread_setname_np(pthread_self(), thread_name);
#endif
}

namespace time {

inline std::string put_time(const tm* t, const char* fmt) {
#ifdef __GNUC__
#include <features.h>
#if __GNUC_PREREQ(5, 0)
    std::stringstream stream;
    stream << std::put_time(t, fmt);
    return stream.str();
#else
    char buf[128];
    if (0 < strftime(buf, sizeof(buf), fmt, t))
        return buf;
    else
        return "";
#endif
#endif
}

inline std::string to_iso_8601(cloud::time t) {
    // convert to time_t which will represent the number of
    // seconds since the UNIX epoch, UTC 00:00:00 Thursday, 1st. January 1970
    auto epoch_seconds = std::chrono::system_clock::to_time_t(t);

    // Format this as date time to seconds resolution
    // e.g. 2016-08-30T08:18:51
    std::stringstream stream;
    stream << time::put_time(gmtime(&epoch_seconds), "%FT%T");

    // If we now convert back to a time_point we will get the time truncated
    // to whole seconds
    auto truncated = std::chrono::system_clock::from_time_t(epoch_seconds);

    // Now we subtract this seconds count from the original time to
    // get the number of extra microseconds..
    auto delta_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t - truncated)
            .count();

    // And append this to the output stream as fractional seconds
    // e.g. 2016-08-30T08:18:51.867479
    stream << "." << std::fixed << std::setw(6) << std::setfill('0')
           << delta_us;

    return stream.str();
}

inline std::string to_iso_8601_packed(cloud::time t) {
    // convert to time_t which will represent the number of
    // seconds since the UNIX epoch, UTC 00:00:00 Thursday, 1st. January 1970
    auto epoch_seconds = std::chrono::system_clock::to_time_t(t);

    // Format this as date time to seconds resolution
    // e.g. 2016-08-30T08:18:51
    std::stringstream stream;
    stream << time::put_time(gmtime(&epoch_seconds), "%Y%m%dT%H%M%S");

    // If we now convert back to a time_point we will get the time truncated
    // to whole seconds
    auto truncated = std::chrono::system_clock::from_time_t(epoch_seconds);

    // Now we subtract this seconds count from the original time to
    // get the number of extra microseconds..
    auto delta_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t - truncated)
            .count();

    // And append this to the output stream as fractional seconds
    // e.g. 2016-08-30T08:18:51.867479
    stream << "." << std::fixed << std::setw(6) << std::setfill('0')
           << delta_us;

    return stream.str();
}

std::string to_iso_packed(cloud::time t) {
    return to_iso_8601_packed(t);
}

std::string now_ISO8601_UTC_packed() {
    return to_iso_8601_packed(std::chrono::system_clock::now());
}

std::string now_ISO8601_UTC() {
    return to_iso_8601(std::chrono::system_clock::now());
}

std::time_t now_time_UTC() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::system_clock::to_time_t(now);
}

std::string time_to_ISO8601(std::time_t t) {
    return to_iso_8601(std::chrono::system_clock::from_time_t(t));
}

std::string time_to_ISO8601_packed(std::time_t t) {
    return to_iso_8601_packed(std::chrono::system_clock::from_time_t(t));
}

inline int parse_int(const char* value) {
    return std::strtol(value, nullptr, 10);
}

std::time_t ISO8601_to_time(const std::string& input) {
    constexpr const size_t expectedLength = sizeof("1234-12-12T12:12:12Z") - 1;
    static_assert(expectedLength == 20, "Unexpected ISO 8601 date/time length");

    if (input.length() < expectedLength) {
        return 0;
    }

    std::tm time = {0};
    time.tm_year = parse_int(&input[0]) - 1900;
    time.tm_mon = parse_int(&input[5]) - 1;
    time.tm_mday = parse_int(&input[8]);
    time.tm_hour = parse_int(&input[11]);
    time.tm_min = parse_int(&input[14]);
    time.tm_sec = parse_int(&input[17]);
    time.tm_isdst = 0;
    // const int millis = input.length() > 20 ? parse_int(&input[20]) : 0;
    // return timegm(&time) * 1000 + millis;
    return timegm(&time);
}

std::string to_iso(cloud::time t) {
    using namespace std::chrono;
    return date::format("%FT%TZ", date::floor<microseconds>(t));
}

std::string to_iso2(cloud::time t) {
    using namespace std::chrono;
    return date::format("%FT%T", date::floor<microseconds>(t));
}

std::string to_iso_local(cloud::time t) {
    using namespace std::chrono;
    return date::format(
        "%FT%TZ",
        date::floor<microseconds>(
            date::make_zoned(date::current_zone(), t).get_local_time()));
}

cloud::time from_double(double t) {
    using namespace std::chrono;
    return system_clock::time_point(
        duration_cast<microseconds>(std::chrono::duration<double>(t)));
}

double to_double(cloud::time t) {
    return std::chrono::duration<double>(t.time_since_epoch()).count();
}

cloud::time from_iso(std::string st) {
    using namespace std::chrono;
    std::istringstream is;
    cloud::time t;
    is.str(st);
    is >> date::parse("%FT%TZ", t);

    if (is.fail())
        return system_clock::from_time_t(0);

    return t;
}

cloud::time from_iso2(std::string st) {
    using namespace std::chrono;
    std::istringstream is;
    cloud::time t;
    is.str(st);
    is >> date::parse("%FT%T", t);

    if (is.fail())
        return system_clock::from_time_t(0);

    return t;
}

cloud::time from_iso_packed(std::string st) {
    using namespace std::chrono;
    std::istringstream is;
    cloud::time t;
    is.str(st);
    is >> date::parse("%Y%m%dT%H%M%S", t);

    if (is.fail())
        return system_clock::from_time_t(0);

    return t;
}

bool iso_time_valid(const std::string& s) {
    using namespace std;
    using namespace date;
    istringstream in {s};
    cloud::time tp;
    in >> date::parse("%FT%TZ", tp);
    return !in.fail();
}

bool is_iso_packed(const std::string& s) {
    // YYYYMMDDThhmmss.mmm
    // YYYYMMDDThhmmss
    std::regex r("([0-9]{8}T[0-9]{6})(.[0-9]+)*");
    return std::regex_match(s, r);
}

bool is_iso(const std::string& s) {
    // 2021-09-15T09:51:20.000000Z
    std::regex r(
        "[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(.[0-9]+)*Z*");
    return std::regex_match(s, r);
}

}  // namespace time

namespace motion {
std::string map::pack(const std::string& unpackedGrid) {
    std::string packedBits;
    ssize_t packedLen;
    map motionMap;

    // +1 for packbits header
    packedBits.resize(unpackedGrid.size() + 1);
    packedLen = packbits((signed char*)unpackedGrid.c_str(),
                         (signed char*)packedBits.data(), unpackedGrid.size());

    if (packedLen > 0) {
        packedBits.resize(packedLen);
        motionMap = base64_encode(packedBits);
    }

    return motionMap;
}

// https://en.wikipedia.org/wiki/PackBits
// There is no way based on the PackBits data to determine the end of the data
// stream; that is to say, one must already know the size of the compressed or
// uncompressed data before reading a PackBits data stream to know where it
// ends.
std::string map::unpack(const std::string& packedGrid, size_t outputLen) {
    std::string unpackedBits;
    std::string packedBits;

    /* Base64 decode */
    packedBits = base64_decode(packedGrid);

    /* Reserve memory for unpacked data, outputLen + '\0', size - 1 to remove
     * trailing '\0' from std::string  */
    unpackedBits.resize(outputLen + 1);
    unpackbits((signed char*)packedBits.c_str(),
               (signed char*)unpackedBits.data(), outputLen);
    unpackedBits[outputLen] = 0;

    return unpackedBits;
}
}  // namespace motion

std::string string_trim(const std::string& name, std::regex regx) {
    std::ostringstream os;

    // VXG Cloud stream name regexp [0-9A-Za-z\\-+]{1,64}
    // remove dots
    os << std::regex_replace(name, regx, "");

    return os.str();
}

std::string ltrim(const std::string& s) {
    static const std::regex lws {"^[[:space:]]*",
                                 std::regex_constants::extended};
    return std::regex_replace(s, lws, "");
}

std::string rtrim(const std::string& s) {
    static const std::regex tws {"[[:space:]]*$",
                                 std::regex_constants::extended};
    return std::regex_replace(s, tws, "");
}

std::string string_trim(const std::string& s) {
    return ltrim(rtrim(s));
}

std::vector<std::string> string_split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

bool string_endswith(std::string const& fullString, std::string const& ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(),
                                        ending.length(), ending));
    } else {
        return false;
    }
}

bool string_startswith(std::string const& fullString,
                       std::string const& start) {
    if (fullString.rfind(start, 0) == 0)
        return true;
    return false;
}

bool string_replace(std::string& str,
                    const std::string& from,
                    const std::string& to) {
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

std::string string_urlencode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n;
         ++i) {
        std::string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char)c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}

inline char from_hex(char ch) {
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

std::string string_urldecode(const std::string& text) {
    char h;
    std::ostringstream escaped;
    escaped.fill('0');

    for (auto i = text.begin(), n = text.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        if (c == '%' && std::distance(i, text.end()) > 2) {
            if (i[1] && i[2]) {
                h = from_hex(i[1]) << 4 | from_hex(i[2]);
                escaped << h;
                i += 2;
            }
        } else if (c == '%') {
            throw std::runtime_error("Lost '%' character");
        } else if (c == '+') {
            escaped << ' ';
        } else {
            escaped << c;
        }
    }

    return escaped.str();
}

std::string string_tolower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string string_toupper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

std::string dirname(const std::string& filepath) {
    char sep = '/';

#ifdef _WIN32
    sep = '\\';
#endif

    size_t i = filepath.rfind(sep, filepath.length());
    if (i != std::string::npos)
        return (filepath.substr(0, i));

    return ("");
}

// Use urlencoded password or password without special symbols
// in case if can't parse url returns original string
std::string hide_password(const std::string& value) {
    const std::string protocol_ending = "://";

    std::size_t found = value.find(protocol_ending);
    if (found == std::string::npos) {
        return value;
    }

    found = value.find(":", found + protocol_ending.length());
    if (found == std::string::npos) {
        return value;
    }
    std::size_t pw_pos = found + 1;

    found = value.find_last_of("@");
    if (found == std::string::npos) {
        return value;
    }

    return std::string(value).replace(pw_pos, found - pw_pos, found - pw_pos,
                                      '*');
}

namespace gcc_abi {
#ifdef __GNUG__
#include <cxxabi.h>
#include <cstdlib>
#include <memory>

std::string demangle(std::string name) {
    int status = -4;  // some arbitrary value to eliminate the compiler warning

    // Only for c++11 and g++
    std::unique_ptr<char, void (*)(void*)> res {
        abi::__cxa_demangle(name.c_str(), NULL, NULL, &status), std::free};

    return (status == 0) ? res.get() : name;
}
#else
// does nothing if not g++
std::string demangle(std::string name) {
    return name;
}
#endif
}  // namespace gcc_abi
}  // namespace utils
}  // namespace cloud
}  // namespace vxg
