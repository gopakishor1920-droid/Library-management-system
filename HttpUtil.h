#ifndef HTTP_UTIL_H
#define HTTP_UTIL_H
#include <string>
#include <map>
#include <sstream>
#include <iomanip>
#include <cctype>

inline std::string jsonEscape(const std::string& s) {
    std::ostringstream out;
    for (unsigned char c : s) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: if (c < 0x20) { out << "?"; } else out << static_cast<char>(c);
        }
    }
    return out.str();
}
inline std::string jsonString(const std::string& s) { return "\"" + jsonEscape(s) + "\""; }
inline std::string jsonGetString(const std::string& body, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    auto p = body.find(marker); if (p == std::string::npos) return "";
    p = body.find(':', p + marker.size()); if (p == std::string::npos) return ""; ++p;
    while (p < body.size() && std::isspace(static_cast<unsigned char>(body[p]))) ++p;
    if (p >= body.size() || body[p] != '"') return "";
    ++p; std::string out; bool escape = false;
    for (; p < body.size(); ++p) {
        char c = body[p];
        if (escape) { if (c == 'n') out += '\n'; else if (c == 'r') out += '\r'; else if (c == 't') out += '\t'; else out += c; escape = false; }
        else if (c == '\\') escape = true;
        else if (c == '"') break;
        else out += c;
    }
    return out;
}
inline int jsonGetInt(const std::string& body, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    auto p = body.find(marker); if (p == std::string::npos) return -1;
    p = body.find(':', p + marker.size()); if (p == std::string::npos) return -1; ++p;
    while (p < body.size() && std::isspace(static_cast<unsigned char>(body[p]))) ++p;
    try { return std::stoi(body.substr(p)); } catch (...) { return -1; }
}
inline std::string urlDecode(const std::string& s) {
    std::string out;
    for (std::size_t i=0; i<s.size(); ++i) {
        if (s[i] == '+') out += ' ';
        else if (s[i] == '%' && i+2 < s.size()) {
            try { out += static_cast<char>(std::stoi(s.substr(i+1,2), nullptr, 16)); i += 2; }
            catch (...) { out += s[i]; }
        } else out += s[i];
    }
    return out;
}
#endif
