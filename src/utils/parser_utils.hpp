#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace pu {
    std::string extractTableNameAfter(const std::string &cmd, const std::string &keyword);
    std::vector<std::string> parseParenList(const std::string &s);
    std::vector<std::string> splitCSVOutsideQuotes(const std::string &s);
    std::pair<std::string,std::string> parseWhereEquals(const std::string &cmd);
    std::unordered_map<std::string,std::string> parseAssignments(const std::string &setPartRaw);
}