#pragma once
#include <string>
#include <vector>

namespace csvu {
    std::vector<std::vector<std::string>> readCSV(const std::string &path);
    void writeCSV(const std::string &path, const std::vector<std::vector<std::string>> &rows);
}