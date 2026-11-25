/*
 * A stand-in for the part of boost::property_tree that Configuration.cpp
 * uses, for the MS-DOS build (DJGPP has no Boost). Found first via -Icompat.
 *
 *   ptree::get<T>(path, default)    path = "section.key"
 *   ptree::put(path, value)
 *   ini_parser::read_ini / write_ini
 */
#ifndef SMB_COMPAT_PTREE_HPP
#define SMB_COMPAT_PTREE_HPP

#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace boost {
namespace property_tree {

class ptree {
public:
    template <typename T>
    T get(const std::string& path, const T& defaultValue) const {
        auto it = values.find(path);
        if (it == values.end()) return defaultValue;
        T result;
        if (!convert(it->second, result)) return defaultValue;
        return result;
    }

    template <typename T>
    void put(const std::string& path, const T& value) {
        std::ostringstream out;
        out << value;
        set(path, out.str());
    }
    void put(const std::string& path, bool value) { set(path, value ? "true" : "false"); }
    void put(const std::string& path, const std::string& value) { set(path, value); }
    void put(const std::string& path, const char* value) { set(path, value); }

    void set(const std::string& path, const std::string& value) {
        if (values.find(path) == values.end()) order.push_back(path);
        values[path] = value;
    }

    // Keys in the order they were first set ("section.key").
    const std::vector<std::string>& keys() const { return order; }
    const std::string& raw(const std::string& path) const { return values.at(path); }

private:
    std::map<std::string, std::string> values;
    std::vector<std::string> order;

    static bool convert(const std::string& s, std::string& out) { out = s; return true; }
    static bool convert(const std::string& s, bool& out) {
        if (s == "true" || s == "1" || s == "yes" || s == "on")   { out = true;  return true; }
        if (s == "false" || s == "0" || s == "no" || s == "off") { out = false; return true; }
        return false;
    }
    template <typename T>
    static bool convert(const std::string& s, T& out) {
        std::istringstream in(s);
        in >> out;
        return !in.fail();
    }
};

} // namespace property_tree
} // namespace boost

#endif
