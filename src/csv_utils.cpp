#include "csv_utils.hpp"
#include "string_utils.hpp"
#include <fstream>

namespace csvu {
    using su::trim;

    std::vector<std::vector<std::string>> readCSV(const std::string &path) {
        std::vector<std::vector<std::string>> rows;
        std::ifstream file(path);
        if (!file.is_open()) return rows;
        std::string line;
        while (std::getline(file, line)) {
            std::vector<std::string> row; std::string cell; bool inD=false;
            for (std::size_t i=0;i<line.size();) {
                if (line[i]=='"') {
                    inD=true; std::string acc; ++i;
                    while (i<line.size()) {
                        if (line[i]=='"') {
                            if (i+1<line.size() && line[i+1]=='"') { acc+='"'; i+=2; }
                            else { ++i; inD=false; break; }
                        } else { acc+=line[i++]; }
                    }
                    row.push_back(acc);
                    if (i<line.size() && line[i]==',') ++i; // skip comma
                } else {
                    std::size_t j=i; while (j<line.size() && line[j]!=',') ++j;
                    row.push_back(trim(line.substr(i, j-i)));
                    i = (j<line.size()? j+1 : j);
                }
            }
            if (!line.empty() && line.back()==',') row.push_back("");
            rows.push_back(row);
        }
        return rows;
    }

    void writeCSV(const std::string &path, const std::vector<std::vector<std::string>> &rows) {
        std::ofstream file(path, std::ios::trunc);
        for (const auto &row : rows) {
            for (std::size_t i=0;i<row.size();++i) {
                std::string cell = row[i]; bool hasComma = (cell.find(',')!=std::string::npos);
                bool hasQuote = (cell.find('"')!=std::string::npos);
                if (hasComma || hasQuote) {
                    std::string esc; esc.reserve(cell.size());
                    for (char c: cell) esc += (c=='"'? std::string("\"\"") : std::string(1,c));
                    file << '"' << esc << '"';
                } else {
                    file << cell;
                }
                if (i+1<row.size()) file << ',';
            }
            file << '\n';
        }
    }
}