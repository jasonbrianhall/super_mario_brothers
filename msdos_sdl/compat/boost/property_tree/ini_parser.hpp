/*
 * INI reading/writing for the compat ptree (see ptree.hpp).
 */
#ifndef SMB_COMPAT_INI_PARSER_HPP
#define SMB_COMPAT_INI_PARSER_HPP

#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "ptree.hpp"

namespace boost {
namespace property_tree {
namespace ini_parser {

inline std::string ini_trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

inline void read_ini(std::istream& in, ptree& tree) {
    std::string line, section;
    while (std::getline(in, line)) {
        line = ini_trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') {
            size_t end = line.find(']');
            section = ini_trim(line.substr(1, end == std::string::npos ? std::string::npos : end - 1));
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = ini_trim(line.substr(0, eq));
        std::string value = ini_trim(line.substr(eq + 1));
        if (value.size() >= 2 && value[0] == '"' && value[value.size() - 1] == '"')
            value = value.substr(1, value.size() - 2);
        tree.set(section.empty() ? key : section + "." + key, value);
    }
}

inline void write_ini(std::ostream& out, const ptree& tree) {
    // Group by section, keeping first-seen order.
    std::vector<std::string> sections;
    for (const std::string& k : tree.keys()) {
        size_t dot = k.find('.');
        std::string sec = dot == std::string::npos ? "" : k.substr(0, dot);
        bool seen = false;
        for (const std::string& s : sections) if (s == sec) { seen = true; break; }
        if (!seen) sections.push_back(sec);
    }
    for (const std::string& sec : sections) {
        if (!sec.empty()) out << "[" << sec << "]\n";
        for (const std::string& k : tree.keys()) {
            size_t dot = k.find('.');
            std::string ksec = dot == std::string::npos ? "" : k.substr(0, dot);
            if (ksec != sec) continue;
            out << (dot == std::string::npos ? k : k.substr(dot + 1)) << "=" << tree.raw(k) << "\n";
        }
        out << "\n";
    }
}

} // namespace ini_parser
} // namespace property_tree
} // namespace boost

#endif
