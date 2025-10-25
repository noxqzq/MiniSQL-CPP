#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace fs = std::filesystem;

class MiniSQL {
private:
    std::string dataDir = "data/";
    std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t");
        size_t end = s.find_last_not_of(" \t");
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    }

public:
    void createTable(const std::string& cmd) {
        size_t pos = cmd.find("TABLE");
        if (pos == std::string::npos) { std::cout << "Syntax error in CREATE TABLE.\n"; return; }

        size_t nameStart = pos + 6;
        size_t parenStart = cmd.find('(', nameStart);
        size_t parenEnd = cmd.find(')', parenStart);
        if (parenStart == std::string::npos || parenEnd == std::string::npos) { std::cout << "Syntax error in CREATE TABLE.\n"; return; }

        std::string tableName = trim(cmd.substr(nameStart, parenStart - nameStart));
        std::string colsPart = cmd.substr(parenStart + 1, parenEnd - parenStart - 1);

        std::istringstream ss(colsPart);
        std::string col;
        std::vector<std::string> columns;
        while (std::getline(ss, col, ',')) { col = trim(col); if (!col.empty()) columns.push_back(col); }

        fs::create_directories(dataDir);
        std::string filePath = dataDir + tableName + ".csv";
        if (fs::exists(filePath)) { std::cout << "Error: Table \"" << tableName << "\" already exists.\n"; return; }

        std::ofstream file(filePath);
        if (!file.is_open()) { std::cout << "Error: Failed to create table file.\n"; return; }

        for (size_t i = 0; i < columns.size(); ++i) { file << columns[i]; if (i < columns.size() - 1) file << ","; }
        file << "\n"; file.close();
        std::cout << "Table \"" << tableName << "\" created successfully.\n";
    }

    void insertIntoTable(const std::string& cmd) {
        size_t intoPos = cmd.find("INTO");
        size_t valuesPos = cmd.find("VALUES");
        if (intoPos == std::string::npos || valuesPos == std::string::npos) { std::cout << "Syntax error in INSERT INTO.\n"; return; }

        std::string tableName = trim(cmd.substr(intoPos + 5, valuesPos - (intoPos + 5)));
        size_t parenStart = cmd.find('(', valuesPos);
        size_t parenEnd = cmd.find(')', parenStart);
        if (parenStart == std::string::npos || parenEnd == std::string::npos) { std::cout << "Syntax error: missing parentheses in VALUES.\n"; return; }

        std::string valuesPart = cmd.substr(parenStart + 1, parenEnd - parenStart - 1);
        std::istringstream ss(valuesPart);
        std::string val;
        std::vector<std::string> values;
        while (std::getline(ss, val, ',')) { val = trim(val); if (!val.empty()) { if (val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2); values.push_back(val); } }

        std::string filePath = dataDir + tableName + ".csv";
        if (!fs::exists(filePath)) { std::cout << "Error: Table \"" << tableName << "\" does not exist.\n"; return; }

        std::ofstream file(filePath, std::ios::app);
        if (!file.is_open()) { std::cout << "Error: Cannot open table for insertion.\n"; return; }

        for (size_t i = 0; i < values.size(); ++i) { file << values[i]; if (i < values.size() - 1) file << ","; }
        file << "\n"; file.close();
        std::cout << "1 row inserted into \"" << tableName << "\".\n";
    }

    void showTable(const std::string& cmd) {
        size_t pos = cmd.find("TABLE");
        if (pos == std::string::npos) { std::cout << "Syntax error in SHOW TABLE.\n"; return; }

        std::string tableName = trim(cmd.substr(pos + 6));
        if (!tableName.empty() && tableName.back() == ';') tableName.pop_back();

        std::string filePath = dataDir + tableName + ".csv";
        if (!fs::exists(filePath)) { std::cout << "Error: Table \"" << tableName << "\" does not exist.\n"; return; }

        std::ifstream file(filePath);
        if (!file.is_open()) { std::cout << "Error: Cannot open table file.\n"; return; }

        std::string line;
        int rowCount = 0;
        std::cout << "+--------------------------------------+\n";
        while (std::getline(file, line)) {
            std::istringstream ss(line);
            std::string cell;
            std::cout << "| ";
            while (std::getline(ss, cell, ',')) std::cout << cell << " | ";
            std::cout << "\n";
            rowCount++;
        }
        std::cout << "+--------------------------------------+\n";
        std::cout << rowCount - 1 << " rows (including header).\n";
        file.close();
    }

    void updateTable(const std::string& cmd) {
        size_t updatePos = cmd.find("UPDATE");
        size_t setPos = cmd.find("SET");
        size_t wherePos = cmd.find("WHERE");
        if (updatePos == std::string::npos || setPos == std::string::npos) { std::cout << "Syntax error in UPDATE.\n"; return; }

        std::string tableName, setPart, wherePart;
        if (wherePos != std::string::npos) {
            tableName = trim(cmd.substr(updatePos + 6, setPos - (updatePos + 6)));
            setPart = trim(cmd.substr(setPos + 3, wherePos - (setPos + 3)));
            wherePart = trim(cmd.substr(wherePos + 5));
        } else {
            tableName = trim(cmd.substr(updatePos + 6, setPos - (updatePos + 6)));
            setPart = trim(cmd.substr(setPos + 3));
        }

        std::string filePath = "data/" + tableName + ".csv";
        if (!fs::exists(filePath)) { std::cout << "Error: Table \"" << tableName << "\" does not exist.\n"; return; }

        std::istringstream ss(setPart);
        std::string assignment;
        std::vector<std::pair<std::string, std::string>> updates;
        while (std::getline(ss, assignment, ',')) {
            assignment = trim(assignment);
            size_t eqPos = assignment.find('=');
            if (eqPos == std::string::npos) { std::cout << "Syntax error in assignment: " << assignment << "\n"; return; }
            std::string column = trim(assignment.substr(0, eqPos));
            std::string value = trim(assignment.substr(eqPos + 1));
            if (!value.empty() && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
            updates.push_back({column, value});
        }

        std::string whereColumn, whereValue;
        if (!wherePart.empty()) {
            size_t eqPos = wherePart.find('=');
            if (eqPos == std::string::npos) { std::cout << "Syntax error in WHERE clause.\n"; return; }
            whereColumn = trim(wherePart.substr(0, eqPos));
            whereValue = trim(wherePart.substr(eqPos + 1));
            if (!whereValue.empty() && whereValue.front() == '"' && whereValue.back() == '"') whereValue = whereValue.substr(1, whereValue.size() - 2);
        }

        std::ifstream inFile(filePath);
        std::vector<std::vector<std::string>> rows;
        std::string line;
        while (std::getline(inFile, line)) {
            std::vector<std::string> row;
            std::istringstream rowStream(line);
            std::string cell;
            while (std::getline(rowStream, cell, ',')) row.push_back(trim(cell));
            rows.push_back(row);
        }
        inFile.close();
        if (rows.empty()) { std::cout << "Error: Table is empty.\n"; return; }

        std::unordered_map<std::string, int> headerIndex;
        for (int i = 0; i < rows[0].size(); ++i) headerIndex[rows[0][i]] = i;

        std::vector<int> updateIndexes;
        for (auto& [col, val] : updates) {
            if (headerIndex.find(col) == headerIndex.end()) { std::cout << "Error: Column \"" << col << "\" does not exist.\n"; return; }
            updateIndexes.push_back(headerIndex[col]);
        }

        int whereIndex = -1;
        if (!whereColumn.empty()) {
            if (headerIndex.find(whereColumn) == headerIndex.end()) { std::cout << "Error: Column \"" << whereColumn << "\" does not exist.\n"; return; }
            whereIndex = headerIndex[whereColumn];
        }

        int updatedCount = 0;
        for (size_t i = 1; i < rows.size(); ++i) {
            bool match = (whereIndex == -1) || (rows[i][whereIndex] == whereValue);
            if (match) { for (size_t j = 0; j < updates.size(); ++j) rows[i][updateIndexes[j]] = updates[j].second; updatedCount++; }
        }

        std::ofstream outFile(filePath);
        for (auto& row : rows) { for (size_t j = 0; j < row.size(); ++j) { outFile << row[j]; if (j < row.size() - 1) outFile << ","; } outFile << "\n"; }
        outFile.close();
        std::cout << "Updated " << updatedCount << " row(s) in \"" << tableName << "\".\n";
    }

    void run() {
        std::cout << "Welcome to MiniSQL-CPP!\n";
        std::cout << "Type \"exit;\" to quit.\n\n";

        std::string input;
        while (true) {
            std::cout << "sql> ";
            std::getline(std::cin, input);
            if (input.empty()) continue;
            if (input == "exit;" || input == "exit") { std::cout << "Goodbye!\n"; break; }

            if (input.rfind("CREATE TABLE", 0) == 0) createTable(input);
            else if (input.rfind("INSERT INTO", 0) == 0) insertIntoTable(input);
            else if (input.rfind("SHOW TABLE", 0) == 0) showTable(input);
            else if (input.rfind("UPDATE", 0) == 0) updateTable(input);
            else std::cout << "Unknown command.\n";
        }
    }
};

int main() {
    MiniSQL sql;
    sql.run();
    return 0;
}
