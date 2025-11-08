#include "table_print.hpp"
#include <iostream>
#include <iomanip>

namespace tp {
    std::vector<std::size_t> computeWidths(const std::vector<std::vector<std::string>>& rows) {
        if (rows.empty()) return {};
        std::vector<std::size_t> w(rows[0].size(), 0);
        for (const auto &row : rows) {
            for (std::size_t c=0;c<row.size();++c) if (row[c].size()>w[c]) w[c]=row[c].size();
        }
        return w;
    }

    static void printPlusDashes(const std::vector<std::size_t>& widths) {
        std::cout << "+";
        for (auto width : widths) {
            std::cout << std::string(width + 2, '-') << "+";
        }
        std::cout << "\n";
    }

    void printBorder(const std::vector<std::size_t>& widths) { printPlusDashes(widths); }

    void printRow(const std::vector<std::string>& row, const std::vector<std::size_t>& widths) {
        std::cout << "|";
        for (std::size_t c=0;c<widths.size();++c) {
            std::cout << ' ' << std::left << std::setw((int)widths[c])
                      << (c<row.size()? row[c] : std::string("")) << '|' ;
        }
        std::cout << "\n";
    }
}