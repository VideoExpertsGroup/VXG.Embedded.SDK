#include <limits>
#include <string>

template <typename T>
T unset_value_for() {
    throw std::exception();
}

template <>
int unset_value_for<int>() {
    return -1;
}

template <>
std::string unset_value_for<std::string>() {
    return std::string("unset");
}

template <>
double unset_value_for<double>() {
    return std::numeric_limits<double>::min();
}

template <typename T>
bool __is_unset(T) {
    return false;
}

template <>
bool __is_unset<int>(int t) {
    return t == -1;
}

template <>
bool __is_unset<std::string>(std::string t) {
    return t == unset_value_for<std::string>();
}

template <>
bool __is_unset<double>(double t) {
    return t == unset_value_for<double>();
}
