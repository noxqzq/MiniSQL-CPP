#include "string_utils.hpp"
#include <cctype>

namespace su {
    static const char* WS = " \t\n\r";

    std::string trim(const std::string &s) {
        std::size_t start = s.find_first_not_of(WS);

        if (start == std::string::npos) 
            return "";

        std::size_t end = s.find_last_not_of(WS);

        return s.substr(start, end - start + 1);
    }

    std::string stripTrailingSemicolon(const std::string &s) {
        std::string out = trim(s);
        if (!out.empty() && out.back() == ';') 
            out.pop_back();

        return trim(out);
    }

    bool startsWithNoCase(const std::string &s, const std::string &prefix) {
        if (prefix.empty()) 
            return true;
        if (s.size() < prefix.size()) 
            return false;
        for (std::size_t i=0;i<prefix.size();++i) {
            if (std::toupper((unsigned char)s[i]) != std::toupper((unsigned char)prefix[i]))
                return false;
        }
        return true;
    }

    std::size_t findNoCase(const std::string &hay, const std::string &needle) {
        if (needle.empty()) 
            return 0;

        if (needle.size() > hay.size()) 
            return std::string::npos;

        for (std::size_t i=0;i+needle.size()<=hay.size();++i) {
            bool match = true;
            for (std::size_t j=0;j<needle.size();++j) {
                if (std::toupper((unsigned char)hay[i+j]) != std::toupper((unsigned char)needle[j])) { 
                    match=false; 
                    break; 
                }
            }
            if (match) 
                return i;
        }
        return std::string::npos;
    }

    std::string cleanLiteral(const std::string &raw) {
        std::string t = stripTrailingSemicolon(trim(raw));
        if (t.size() >= 2) {
            bool dbl = (t.front()=='"' && t.back()=='"');
            bool sgl = (t.front()=='\'' && t.back()=='\'');
            if (dbl || sgl) 
                t = t.substr(1, t.size()-2);
        }
        return trim(t);
    }
}