#pragma once
#include <string>
#include <cstddef>

namespace su {
    std::string trim(const std::string& s);
    std::string stripTrailingSemicolon(const std::string& s);
    bool startsWithNoCase(const std::string& s, const std::string& prefix);
    std::size_t findNoCase(const std::string& hay, const std::string& needle);
    std::string cleanLiteral(const std::string& raw);
}