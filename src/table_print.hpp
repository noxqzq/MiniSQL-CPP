#pragma once
#include <string>
#include <vector>

namespace tp {
    // Computes column widths from rows (row[0] is header)
    std::vector<std::size_t> computeWidths(const std::vector<std::vector<std::string>>& rows);

    // Print a single box-drawn row given widths
    void printBorder(const std::vector<std::size_t>& widths);
    void printRow(const std::vector<std::string>& row, const std::vector<std::size_t>& widths);
}