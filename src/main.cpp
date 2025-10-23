#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

class MiniSQL {
private:
    std::string dataDir = "data/";

    // === Helper: trim whitespace ===
    std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t");
        size_t end = s.find_last_not_of(" \t");
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    }

public:
    // === CREATE TABLE ===
    void createTable(const std::string& cmd) {
        size_t pos = cmd.find("TABLE");
        if (pos == std::string::npos) {
            std::cout << "Syntax error in CREATE TABLE.\n";
            return;
        }

        // Parse table name
        size_t nameStart = pos + 6;
        size_t parenStart = cmd.find('(', nameStart);
        size_t parenEnd = cmd.find(')', parenStart);
        if (parenStart == std::string::npos || parenEnd == std::string::npos) {
            std::cout << "Syntax error in CREATE TABLE.\n";
            return;
        }

        std::string tableName = trim(cmd.substr(nameStart, parenStart - nameStart));
        std::string colsPart = cmd.substr(parenStart + 1, parenEnd - parenStart - 1);

        // Split columns
        std::istringstream ss(colsPart);
        std::string col;
        std::vector<std::string> columns;
        while (std::getline(ss, col, ',')) {
            col = trim(col);
            if (!col.empty()) columns.push_back(col);
        }

        // Make data directory if needed
        fs::create_directories(dataDir);

        // Create CSV file path
        std::string filePath = dataDir + tableName + ".csv";

        // Check if file exists
        if (fs::exists(filePath)) {
            std::cout << "Error: Table \"" << tableName << "\" already exists.\n";
            return;
        }

        // Write header line (column names)
        std::ofstream file(filePath);
        if (!file.is_open()) {
            std::cout << "Error: Failed to create table file.\n";
            return;
        }
        for (size_t i = 0; i < columns.size(); ++i) {
            file << columns[i];
            if (i < columns.size() - 1) file << ",";
        }
        file << "\n";
        file.close();

        std::cout << "Table \"" << tableName << "\" created successfully.\n";
    }

    // === INSERT INTO TABLE ===
    void insertIntoTable(const std::string& cmd) {

        size_t intoPos = cmd.find("INTO");
        size_t valuesPos = cmd.find("VALUES");

        
        if (intoPos == std::string::npos || valuesPos == std::string::npos) {
            std::cout << "Syntax error in INSERT INTO.\n";
            return;
        }

        std::string tableName = trim(cmd.substr(intoPos + 5, valuesPos - (intoPos + 5)));
        std::string valuesPart;

        size_t parenStart = cmd.find('(', valuesPos);
        size_t parenEnd = cmd.find(')', parenStart);
        if (parenStart == std::string::npos || parenEnd == std::string::npos) {
            std::cout << "Syntax error: missing parentheses in VALUES.\n";
            return;
        }

        valuesPart = cmd.substr(parenStart + 1, parenEnd - parenStart - 1);

        std::istringstream ss(valuesPart);
        std::string val;
        std::vector<std::string> values;
        while(std::getline(ss, val, ',')) {
            val = trim(val);
            if(!val.empty()) {
                if(val.front() == '"' && val.back() == '"')
                    val = val.substr(1, val.size() - 2);
                values.push_back(val);
            }
        }

        // File path
        std::string filePath = dataDir + tableName + ".csv";
        if (!fs::exists(filePath)) {
            std::cout << "Error: Table \"" << tableName << "\" does not exist.\n";
            return;
        }

        // Append to CSV
        std::ofstream file(filePath, std::ios::app);
        if (!file.is_open()) {
            std::cout << "Error: Cannot open table for insertion.\n";
            return;
        }

        for (size_t i = 0; i < values.size(); ++i) {
            file << values[i];
            if (i < values.size() - 1) file << ",";
        }
        file << "\n";
        file.close();

        std::cout << "1 row inserted into \"" << tableName << "\".\n";
    }

        // === SHOW TABLE ===
    void showTable(const std::string& cmd) {
        size_t pos = cmd.find("TABLE");
        if (pos == std::string::npos) {
            std::cout << "Syntax error in SHOW TABLE.\n";
            return;
        }

        std::string tableName = trim(cmd.substr(pos + 6));
        if (!tableName.empty() && tableName.back() == ';') tableName.pop_back();

        std::string filePath = dataDir + tableName + ".csv";
        if (!fs::exists(filePath)) {
            std::cout << "Error: Table \"" << tableName << "\" does not exist.\n";
            return;
        }

        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cout << "Error: Cannot open table file.\n";
            return;
        }

        std::string line;
        int rowCount = 0;
        std::cout << "+--------------------------------------+\n";
        while (std::getline(file, line)) {
            std::istringstream ss(line);
            std::string cell;
            std::cout << "| ";
            while (std::getline(ss, cell, ',')) {
                std::cout << cell << " | ";
            }
            std::cout << "\n";
            rowCount++;
        }
        std::cout << "+--------------------------------------+\n";
        std::cout << rowCount - 1 << " rows (including header).\n";
        file.close();
    }

    // === MAIN LOOP ===
    void run() {
        std::cout << "Welcome to MiniSQL-CPP!\n";
        std::cout << "Type \"exit;\" to quit.\n\n";

        std::string input;
        while (true) {
            std::cout << "sql> ";
            std::getline(std::cin, input);

            if (input.empty()) continue;
            if (input == "exit;" || input == "exit") {
                std::cout << "Goodbye!\n";
                break;
            }

            if (input.rfind("CREATE TABLE", 0) == 0)
                createTable(input);
            else if (input.rfind("INSERT INTO", 0) == 0)
                insertIntoTable(input);
            else if (input.rfind("SHOW TABLE", 0) == 0)
                showTable(input);
            else
                std::cout << "Unknown command.\n";
        }
    }
};

int main() {
    MiniSQL sql;
    sql.run();
    return 0;
}