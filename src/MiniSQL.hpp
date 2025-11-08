#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

class MiniSQL {
private:
    fs::path dataRoot;

    // internal helpers
    std::vector<std::vector<std::string>> loadTable(const std::string &tableName);
    void saveTable(const std::string &tableName, const std::vector<std::vector<std::string>> &rows);

    // command handlers
    void createTable(const std::string &cmdRaw);
    void insertIntoTable(const std::string &cmdRaw);
    void updateTable(const std::string &cmdRaw);
    void deleteFromTable(const std::string &cmdRaw);
    void dropTable(const std::string &cmdRaw);
    void alterTable(const std::string &cmdRaw);
    void showTable(const std::string &cmdRaw);
    void showPath();
    void selectTable(const std::string &cmdRaw); // UPDATED formatting

public:
    explicit MiniSQL(const fs::path &exePath);
    void run();
};