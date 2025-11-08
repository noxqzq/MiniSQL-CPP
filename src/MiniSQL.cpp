#include "MiniSQL.hpp"
#include "string_utils.hpp"
#include "parser_utils.hpp"
#include "csv_utils.hpp"
#include "table_print.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <limits>

using su::trim; using su::stripTrailingSemicolon; using su::startsWithNoCase; using su::findNoCase;

// ---------- CSV I/O wrappers ----------
std::vector<std::vector<std::string>> MiniSQL::loadTable(const std::string &tableName) {
    fs::path p = dataRoot / (tableName + ".csv");
    return csvu::readCSV(p.string());
}

void MiniSQL::saveTable(const std::string &tableName, const std::vector<std::vector<std::string>> &rows) {
    fs::path p = dataRoot / (tableName + ".csv");
    csvu::writeCSV(p.string(), rows);
}

// ---------- Commands ----------
void MiniSQL::createTable(const std::string &cmdRaw) {
    std::string cmd = stripTrailingSemicolon(cmdRaw);
    std::size_t tableKW = findNoCase(cmd, "TABLE");
    if (tableKW == std::string::npos) { std::cout << "Syntax error: missing keyword TABLE.\n"; return; }
    std::size_t open = cmd.find('(', tableKW);
    if (open == std::string::npos) { std::cout << "Syntax error: column list required in parentheses.\n"; return; }
    std::size_t close = cmd.find(')', open+1);
    if (close == std::string::npos) { std::cout << "Syntax error: missing closing ')'.\n"; return; }

    std::string between = trim(cmd.substr(tableKW+5, open-(tableKW+5)));
    std::string tableName = pu::extractTableNameAfter("TABLE "+between, "TABLE");
    if (tableName.empty()) { std::cout << "Syntax error: missing table name.\n"; return; }

    std::vector<std::string> cols = pu::parseParenList(cmd.substr(open, close-open+1));
    if (cols.empty()) { std::cout << "No columns specified.\n"; return; }

    fs::path p = dataRoot / (tableName + ".csv");
    if (fs::exists(p)) { std::cout << "Table \""<<tableName<<"\" already exists.\n"; return; }

    std::vector<std::vector<std::string>> rows; rows.push_back(cols);
    saveTable(tableName, rows);
    std::cout << "Created table \""<<tableName<<"\" with "<<cols.size()<<" column(s).\n";
}

void MiniSQL::insertIntoTable(const std::string &cmdRaw) {
    std::string cmd = stripTrailingSemicolon(cmdRaw);
    std::string tableName = pu::extractTableNameAfter(cmd, "INTO");
    if (tableName.empty()) { std::cout << "Syntax error: missing table name in INSERT.\n"; return; }
    std::size_t valPos = findNoCase(cmd, "VALUES");
    if (valPos==std::string::npos) { std::cout << "Syntax error: missing VALUES in INSERT.\n"; return; }
    std::vector<std::string> values = pu::parseParenList(trim(cmd.substr(valPos+6)));

    auto rows = loadTable(tableName);
    if (rows.empty()) { std::cout << "Table \""<<tableName<<"\" not found or empty. Create it first.\n"; return; }
    const auto &header = rows[0];
    if (values.size()!=header.size()) {
        std::cout << "Column count mismatch: expected "<<header.size()<<" values, got "<<values.size()<<".\n"; return;
    }
    rows.push_back(values); saveTable(tableName, rows);
    std::cout << "Inserted 1 row into \""<<tableName<<"\".\n";
}

void MiniSQL::updateTable(const std::string &cmdRaw) {
    std::string cmd = stripTrailingSemicolon(cmdRaw);
    std::string tableName = pu::extractTableNameAfter(cmd, "UPDATE");
    if (tableName.empty()) { std::cout << "Syntax error: missing table name in UPDATE.\n"; return; }
    std::size_t setPos = findNoCase(cmd, "SET");
    if (setPos==std::string::npos) { std::cout << "Syntax error: missing SET in UPDATE.\n"; return; }

    std::string setAndRest = trim(cmd.substr(setPos));
    std::size_t wherePos = findNoCase(setAndRest, "WHERE");
    std::string setPart = (wherePos==std::string::npos ? setAndRest : trim(setAndRest.substr(0, wherePos)));

    auto assigns = pu::parseAssignments(setPart);
    auto [wcol, wval] = pu::parseWhereEquals(cmd);

    auto rows = loadTable(tableName);
    if (rows.empty()) { std::cout << "Table \""<<tableName<<"\" not found or empty.\n"; return; }

    const auto &header = rows[0]; std::unordered_map<std::string,std::size_t> idx;
    for (std::size_t i=0;i<header.size();++i) idx[header[i]]=i;
    for (auto &kv : assigns) if (!idx.count(kv.first)) { std::cout << "Unknown column in SET: "<<kv.first<<"\n"; return; }

    std::size_t whereIdx = (std::size_t)-1;
    if (!wcol.empty()) {
        if (!idx.count(wcol)) { std::cout << "Unknown column in WHERE: "<<wcol<<"\n"; return; }
        whereIdx = idx[wcol];
    }

    int updated=0;
    for (std::size_t r=1;r<rows.size();++r) {
        bool match = (whereIdx==(std::size_t)-1) || (rows[r][whereIdx]==wval);
        if (match) { for (auto &kv: assigns) rows[r][idx[kv.first]] = kv.second; ++updated; }
    }
    saveTable(tableName, rows);
    std::cout << "Updated "<<updated<<" row(s) in \""<<tableName<<"\".\n";
}

void MiniSQL::deleteFromTable(const std::string &cmdRaw) {
    std::string cmd = stripTrailingSemicolon(cmdRaw);
    std::string tableName = pu::extractTableNameAfter(cmd, "FROM");
    if (tableName.empty()) { std::cout << "Syntax error: missing table name in DELETE.\n"; return; }

    auto whereKV = pu::parseWhereEquals(cmd);
    auto rows = loadTable(tableName);

    if (whereKV.first.empty()) {
        char choice; std::cout << "WARNING: This will delete ALL records from table \""<<tableName<<"\"!\n";
        std::cout << "Are you sure you want to continue? (Y/N): ";
        std::cin >> choice; std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        if (choice=='y'||choice=='Y') { std::vector<std::vector<std::string>> newRows; newRows.push_back(rows[0]); saveTable(tableName,newRows); std::cout<<"All records deleted from \""<<tableName<<"\".\n"; }
        else { std::cout << "Operation cancelled.\n"; }
        return;
    }

    if (rows.empty()) { std::cout << "Table \""<<tableName<<"\" not found or empty.\n"; return; }

    const auto &header = rows[0]; std::size_t colIndex=(std::size_t)-1;
    for (std::size_t i=0;i<header.size();++i) if (header[i]==whereKV.first) { colIndex=i; break; }
    if (colIndex==(std::size_t)-1) { std::cout << "Unknown column in WHERE: "<<whereKV.first<<"\n"; return; }

    std::vector<std::vector<std::string>> newRows; newRows.push_back(header); int deleted=0;
    for (std::size_t i=1;i<rows.size();++i) { if (rows[i][colIndex]==whereKV.second) ++deleted; else newRows.push_back(rows[i]); }
    saveTable(tableName, newRows);
    std::cout << "Deleted "<<deleted<<" row(s) from \""<<tableName<<"\".\n";
}

void MiniSQL::dropTable(const std::string &cmdRaw) {
    std::string cmd = stripTrailingSemicolon(cmdRaw);
    std::string tableName = pu::extractTableNameAfter(cmd, "TABLE");
    if (tableName.empty()) { std::cout << "Syntax error: missing table name in DROP"; return; }
    auto rows = loadTable(tableName);
    if (rows.empty()) { std::cout << "Table \""<<tableName<<"\" not found.\n"; return; }
    fs::path p = dataRoot / (tableName + ".csv");
    if (fs::remove(p)) std::cout << "File '"<<p<<"' deleted successfully."<<std::endl;
    else std::cout << "File '"<<p<<"' not found or could not be deleted."<<std::endl;
}

void MiniSQL::alterTable(const std::string &cmdRaw) {
    std::string cmd = stripTrailingSemicolon(cmdRaw);
    std::string tableName = pu::extractTableNameAfter(cmd, "TABLE");
    if (tableName.empty()) { std::cout << "Syntax error: missing table name in ALTER. \n"; return; }
    auto rows = loadTable(tableName);
    if (rows.empty()) { std::cout << "Table \""<<tableName<<"\" not found or empty.\n"; return; }

    std::size_t addPos = findNoCase(cmd, "ADD");
    std::size_t dropPos = findNoCase(cmd, "DROP");
    if (addPos!=std::string::npos && dropPos!=std::string::npos) { std::cout << "Syntax error: cannot use both ADD and DROP in one command.\n"; return; }
    if (addPos==std::string::npos && dropPos==std::string::npos) { std::cout << "Syntax error: expected ADD or DROP after table name.\n"; return; }

    if (addPos!=std::string::npos) {
        std::string newCol = su::stripTrailingSemicolon(trim(cmd.substr(addPos+3)));
        if (newCol.empty()) { std::cout << "Syntax error: missing column name for ADD.\n"; return; }
        for (const auto &c: rows[0]) if (c==newCol) { std::cout << "Column \""<<newCol<<"\" already exists.\n"; return; }
        for (std::size_t r=0;r<rows.size();++r) rows[r].push_back(r==0? newCol : "");
        saveTable(tableName, rows); std::cout << "Added column \""<<newCol<<"\" to table \""<<tableName<<"\".\n";
    } else {
        std::string dropCol = su::stripTrailingSemicolon(trim(cmd.substr(dropPos+4)));
        if (dropCol.empty()) { std::cout << "Syntax error: mssing column name for DROP.\n"; return; }
        auto &header = rows[0]; std::size_t colIndex=(std::size_t)-1;
        for (std::size_t i=0;i<header.size();++i) if (header[i]==dropCol) { colIndex=i; break; }
        if (colIndex==(std::size_t)-1) { std::cout << "Unknown column: "<<dropCol<<"\n"; return; }
        for (auto &row: rows) if (colIndex<row.size()) row.erase(row.begin()+colIndex);
        saveTable(tableName, rows); std::cout << "Dropped column \""<<dropCol<<"\" from table \""<<tableName<<"\".\n";
    }
}

void MiniSQL::showTable(const std::string &cmdRaw) {
    std::string tableName = pu::extractTableNameAfter(cmdRaw, "TABLE");
    auto rows = loadTable(tableName);
    if (rows.empty()) { std::cout << "Table \""<<tableName<<"\" not found or empty.\n"; return; }

    auto widths = tp::computeWidths(rows);
    tp::printBorder(widths);
    tp::printRow(rows[0], widths);
    tp::printBorder(widths);
    for (std::size_t r=1;r<rows.size();++r) tp::printRow(rows[r], widths);
    tp::printBorder(widths);
    std::cout << (rows.size()-1) << " row(s).\n";
}

void MiniSQL::showPath() {
    std::cout << "Current working directory: " << fs::current_path().string() << "\n";
    std::cout << "Data directory:           " << dataRoot.string() << "\n";
}

// ============ UPDATED SELECT (box-style output) ============
void MiniSQL::selectTable(const std::string &cmdRaw) {
    std::string cmd = stripTrailingSemicolon(cmdRaw);
    std::size_t selectPos = findNoCase(cmd, "SELECT");
    std::size_t fromPos   = findNoCase(cmd, "FROM");
    if (selectPos==std::string::npos || fromPos==std::string::npos) { std::cout << "Syntax error: malformed SELECT statement.\n"; return; }

    std::string selectPart = trim(cmd.substr(selectPos+6, fromPos-(selectPos+6)));
    std::string afterFrom  = trim(cmd.substr(fromPos+4));
    std::string tableName  = pu::extractTableNameAfter(afterFrom, "");
    if (tableName.empty()) { std::cout << "Syntax error: missing table name in SELECT.\n"; return; }

    auto rows = loadTable(tableName);
    if (rows.empty()) { std::cout << "Table \""<<tableName<<"\" not found or empty.\n"; return; }

    std::vector<std::string> headers = rows[0];
    std::vector<std::vector<std::string>> data(rows.begin()+1, rows.end());

    std::unordered_map<std::string,std::size_t> colIndex;
    for (std::size_t i=0;i<headers.size();++i) colIndex[headers[i]]=i;

    auto [whereCol, whereVal] = pu::parseWhereEquals(cmd);
    bool hasWhere = !whereCol.empty();

    std::vector<std::string> selectCols;
    if (trim(selectPart)=="*") selectCols = headers;
    else selectCols = pu::parseParenList("("+selectPart+")");

    for (const auto &col : selectCols) {
        if (!colIndex.count(col)) { std::cout << "Error: unknown column \""<<col<<"\".\n"; return; }
    }

    // Build an in-memory table for printing: header + filtered rows with only selected columns
    std::vector<std::vector<std::string>> printable;
    printable.push_back(selectCols);

    for (const auto &row : data) {
        if (row.size()!=headers.size()) continue;
        if (hasWhere) {
            auto it = colIndex.find(whereCol);
            if (it==colIndex.end()) { std::cout << "Error: unknown column in WHERE clause \""<<whereCol<<"\".\n"; return; }
            if (row[it->second] != whereVal) continue;
        }
        std::vector<std::string> projected; projected.reserve(selectCols.size());
        for (const auto &c : selectCols) projected.push_back(row[colIndex[c]]);
        printable.push_back(projected);
    }

    auto widths = tp::computeWidths(printable);
    tp::printBorder(widths);           // +----+------+-----+
    tp::printRow(printable[0], widths);// | id | name | age |
    tp::printBorder(widths);           // +----+------+-----+
    if (printable.size()==1) {
        // no data rows → print bottom border identical to separator (matches your example)
        tp::printBorder(widths);
        return;
    }
    for (std::size_t r=1;r<printable.size();++r) tp::printRow(printable[r], widths);
    tp::printBorder(widths);           // bottom border
}

// ---------- lifecycle ----------
MiniSQL::MiniSQL(const fs::path &exePath) {
    const char *envDir = std::getenv("MINISQL_DATA");
    if (envDir) { dataRoot = fs::weakly_canonical(fs::absolute(fs::path(envDir))); }
    else {
        fs::path exeAbs = fs::weakly_canonical(fs::absolute(exePath));
        fs::path exeDir = exeAbs.parent_path();
        dataRoot = fs::weakly_canonical(exeDir / "data");
    }
    if (!fs::exists(dataRoot)) fs::create_directories(dataRoot);
    std::cout << "[MiniSQL] Using data directory: "<<dataRoot.string()<<"\n";
    std::cout << "[MiniSQL] Current working directory: "<<fs::current_path().string()<<"\n";
}

void MiniSQL::run() {
    std::cout << "Welcome to MiniSQL-CPP!\n";
    std::cout << "Commands end with ';'. Supported: CREATE, INSERT, UPDATE, DELETE, SHOW, SHOW PATH, EXIT, ALTER, DROP, SELECT\n\n";
    std::string accum;
    while (true) {
        std::cout << "sql> ";
        std::string line; if (!std::getline(std::cin, line)) break;
        accum += line + "\n";
        if (accum.find(';')==std::string::npos) continue;
        std::size_t semi = accum.find(';');
        std::string input = trim(accum.substr(0, semi+1));
        accum = trim(accum.substr(semi+1));
        if (input.empty()) continue;

        if (startsWithNoCase(input, "EXIT")) break;
        else if (startsWithNoCase(input, "CREATE TABLE")) createTable(input);
        else if (startsWithNoCase(input, "INSERT INTO"))  insertIntoTable(input);
        else if (startsWithNoCase(input, "UPDATE"))       updateTable(input);
        else if (startsWithNoCase(input, "DELETE FROM"))  deleteFromTable(input);
        else if (startsWithNoCase(input, "ALTER TABLE"))  alterTable(input);
        else if (startsWithNoCase(input, "SHOW TABLE"))   showTable(input);
        else if (startsWithNoCase(input, "SHOW PATH"))    showPath();
        else if (startsWithNoCase(input, "DROP TABLE"))   dropTable(input);
        else if (startsWithNoCase(input, "SELECT"))       selectTable(input);
        else std::cout << "Unknown command.\n";
    }
    std::cout << "Goodbye!\n";
}