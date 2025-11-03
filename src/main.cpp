// MiniSQL-CPP (portable, beginner-friendly)
// - CSV-backed toy database with basic SQL-like commands
// - Stores data in a stable directory that works on any machine
// - Default: "data" folder located next to the executable
// - Override with environment variable MINISQL_DATA
//
// Commands (end each with a semicolon ';'):
//   CREATE TABLE <name> (col1, col2, ...);
//   INSERT INTO <name> VALUES (v1, v2, ...);
//   UPDATE <name> SET col=val, col2="val2" WHERE key="something";
//   DELETE FROM <name> WHERE col = value;
//   ALTER TABLE <name> ADD/DROP <column name>;
//   SHOW TABLE <name>;
//   SHOW PATH;    // prints CWD and resolved data directory
//   EXIT;
//
// Parsing notes:
// - Values may be 'single' or "double" quoted; commas inside quotes are supported.
// - This is intentionally simple; no type system or schema enforcement beyond column count.

#include <filesystem>    // std::filesystem::path, exists, create_directories, current_path; we alias as fs
#include <fstream>       // std::ifstream, std::ofstream for reading / writing CSV files
#include <iomanip>       // std::setw, std::left for aligned table output
#include <iostream>      // std::cout, std::cin for console I/O
#include <sstream>       // std::stringstream for basic CSV parsing per line
#include <string>        // std::string, std::getline, string utilities
#include <unordered_map> // std::unordered_map to map column names to indices
#include <vector>        // std::vector to store table rows in memory
#include <cctype>        // std::toupper for case-insensitive comparisons
#include <cstdlib>       // std::getenv to allow env override for data directory

namespace fs = std::filesystem; // alias filesystem namespace for brevity

// -------------------- MiniSQL class --------------------
class MiniSQL {
private:
    fs::path dataRoot; // absolute folder where we read/write CSV files

    // --------------- String helpers ---------------

    static std::string trim(const std::string &s) {
        const char *whiteSpace = " \t\n\r";                     // whitespace chars to remove
        size_t start = s.find_first_not_of(whiteSpace);         // find first non-ws
        if (start == std::string::npos) {                       // if all whitespace
            return "";                                          // return empty
        }
        size_t end = s.find_last_not_of(whiteSpace);            // find last non-ws
        return s.substr(start, end - start + 1);                // slice the trimmed substring
    }

    static std::string stripTrailingSemicolon(const std::string &s) {
        std::string out = trim(s);                              // trim spaces first
        if (!out.empty()) {                                     // if not empty
            if (out.back() == ';') {                            // if last char is ';'
                out.pop_back();                                 // remove it
            }
        }
        return trim(out);                                       // trim again and return
    }

    static bool startsWithNoCase(const std::string &s, const std::string &prefix) {
        if (s.size() < prefix.size()) {                         // if shorter than prefix
            return false;                                       // cannot start with it
        }
        for (size_t i = 0; i < prefix.size(); ++i) {            // compare each character
            if (std::toupper((unsigned char)s[i]) !=            // uppercase lhs
                std::toupper((unsigned char)prefix[i])) {       // uppercase rhs
                return false;                                   // mismatch
            }
        }
        return true;                                            // all matched
    }

    // Manual case-insensitive find (simple alternative to std::search)
    static size_t findNoCase(const std::string &hay, const std::string &needle) {
        if (needle.empty()) {                                   // empty needle
            return 0;                                           // found at 0
        }
        if (needle.size() > hay.size()) {                       // longer than haystack
            return std::string::npos;                           // cannot be found
        }
        for (size_t i = 0; i + needle.size() <= hay.size(); ++i) { // slide window over haystack
            bool match = true;                                  // assume match until disproved
            for (size_t j = 0; j < needle.size(); ++j) {        // compare each char
                if (std::toupper((unsigned char)hay[i + j]) !=  // uppercase hay char
                    std::toupper((unsigned char)needle[j])) {   // uppercase needle char
                    match = false;                              // mark mismatch
                    break;                                      // stop inner loop
                }
            }
            if (match) {                                        // if all matched
                return i;                                       // return start index
            }
        }
        return std::string::npos;                               // not found
    }

    static std::string extractTableNameAfter(const std::string &cmd, const std::string &keyword) {
        size_t pos = findNoCase(cmd, keyword);                  // find keyword position
        if (pos == std::string::npos) {                         // if not found
            return "";                                          // return empty
        }
        std::string rest = trim(cmd.substr(pos + keyword.size())); // take text after keyword
        size_t end = rest.find_first_of(" \t\n\r(),;");         // stop at separators
        if (end == std::string::npos) {                         // if no separator
            return stripTrailingSemicolon(rest);                // clean and return
        } else {
            return stripTrailingSemicolon(rest.substr(0, end)); // clean and return slice
        }
    }

    // Remove surrounding quotes if present, handle semicolon, final trim
    static std::string cleanLiteral(const std::string &raw) {
        std::string t = stripTrailingSemicolon(trim(raw));      // trim and drop trailing ';'
        if (t.size() >= 2) {                                    // if long enough to have quotes
            bool dbl = (t.front() == '"' && t.back() == '"');   // check double quotes
            bool sgl = (t.front() == '\'' && t.back() == '\''); // check single quotes
            if (dbl || sgl) {                                   // if quoted
                t = t.substr(1, t.size() - 2);                  // remove quotes
            }
        }
        return trim(t);                                         // return final trimmed value
    }

    // Parse "(a, "b, c", 'd')" into tokens, respecting quotes and commas inside quotes
    static std::vector<std::string> parseParenList(const std::string &s) {
        std::vector<std::string> out;                           // output tokens
        std::string work = trim(s);                             // local working copy
        if (!work.empty()) {                                    // if not empty
            if (work.front() == '(' && work.back() == ')') {    // if wrapped in parentheses
                work = work.substr(1, work.size() - 2);         // remove them
            }
        }

        std::string token;                                      // current token buffer
        bool inSingle = false;                                  // single-quote state
        bool inDouble = false;                                  // double-quote state
        for (size_t i = 0; i < work.size(); ++i) {              // iterate characters
            char c = work[i];                                   // current char
            if (c == '"' && !inSingle) {                        // toggle double quotes if not in single
                inDouble = !inDouble;                           // flip state
                token += c;                                     // keep quote in token for now
            } else if (c == '\'' && !inDouble) {                // toggle single quotes if not in double
                inSingle = !inSingle;                           // flip state
                token += c;                                     // keep quote in token for now
            } else if (c == ',' && !inSingle && !inDouble) {    // comma outside quotes => split
                out.push_back(cleanLiteral(token));             // push cleaned token
                token.clear();                                  // reset buffer
            } else {                                            // regular character
                token += c;                                     // append to token
            }
        }
        if (!token.empty() || work.empty()) {                   // push last token or empty list case
            out.push_back(cleanLiteral(token));                 // push cleaned final token
        }
        return out;                                             // return tokens
    }

    // Split "a=1, b='x', c=\"y\"" into ["a=1","b='x'","c=\"y\""] respecting quotes
    static std::vector<std::string> splitCSVOutsideQuotes(const std::string &s) {
        std::vector<std::string> out;                           // output pieces
        std::string token;                                      // current buffer
        bool inSingle = false;                                  // single-quote state
        bool inDouble = false;                                  // double-quote state
        for (size_t i = 0; i < s.size(); ++i) {                 // iterate characters
            char c = s[i];                                      // current char
            if (c == '"' && !inSingle) {                        // toggle double quotes
                inDouble = !inDouble;                           // flip state
                token += c;                                     // keep quote for now
            } else if (c == '\'' && !inDouble) {                // toggle single quotes
                inSingle = !inSingle;                           // flip state
                token += c;                                     // keep quote for now
            } else if (c == ',' && !inSingle && !inDouble) {    // comma outside quotes => split
                out.push_back(trim(token));                     // push trimmed piece
                token.clear();                                  // reset buffer
            } else {                                            // regular char
                token += c;                                     // append to token
            }
        }
        if (!token.empty()) {                                   // if leftover token
            out.push_back(trim(token));                         // push it
        }
        return out;                                             // return pieces
    }

    // Parse WHERE col = value (quotes allowed)
    static std::pair<std::string, std::string> parseWhereEquals(const std::string &cmd) {
        size_t wherePos = findNoCase(cmd, "WHERE");             // locate WHERE
        if (wherePos == std::string::npos) {                    // if not found
            return {"", ""};                                    // return empty pair
        }
        std::string wherePart = stripTrailingSemicolon(         // slice after WHERE and clean
            trim(cmd.substr(wherePos + 5))
        );

        bool inSingle = false;                                  // single-quote state
        bool inDouble = false;                                  // double-quote state
        size_t eqPos = std::string::npos;                       // position of '=' outside quotes
        for (size_t i = 0; i < wherePart.size(); ++i) {         // scan characters
            char c = wherePart[i];                              // current char
            if (c == '"' && !inSingle) {                        // toggle double-quote state
                inDouble = !inDouble;                           // flip
            } else if (c == '\'' && !inDouble) {                // toggle single-quote state
                inSingle = !inSingle;                           // flip
            } else if (c == '=' && !inSingle && !inDouble) {    // found '=' outside quotes
                eqPos = i;                                      // record position
                break;                                          // stop scanning
            }
        }
        if (eqPos == std::string::npos) {                       // if no '=' found
            return {"", ""};                                    // return empty pair
        }

        std::string col = trim(wherePart.substr(0, eqPos));     // left side = column
        std::string val = cleanLiteral(wherePart.substr(eqPos + 1)); // right side = value cleaned
        return {col, val};                                      // return pair
    }

    // Parse "SET a=1, b='x'" into a map {a:"1", b:"x"}
    static std::unordered_map<std::string, std::string> parseAssignments(const std::string &setPartRaw) {
        std::unordered_map<std::string, std::string> out;       // output assignments map
        std::string setPart = setPartRaw;                       // local copy
        size_t setPos = findNoCase(setPart, "SET");             // find SET keyword
        if (setPos != std::string::npos) {                      // if found
            setPart = setPart.substr(setPos + 3);               // drop "SET"
        }
        setPart = stripTrailingSemicolon(trim(setPart));        // clean spaces and semicolon

        std::vector<std::string> pieces = splitCSVOutsideQuotes(setPart); // split by comma outside quotes
        for (size_t k = 0; k < pieces.size(); ++k) {            // iterate pieces
            std::string piece = pieces[k];                      // current "key=value"
            bool inSingle = false;                              // single-quote state
            bool inDouble = false;                              // double-quote state
            size_t eqPos = std::string::npos;                   // '=' position outside quotes

            for (size_t i = 0; i < piece.size(); ++i) {         // scan chars to find '='
                char c = piece[i];                              // current char
                if (c == '"' && !inSingle) {                    // toggle double-quote
                    inDouble = !inDouble;                       // flip
                } else if (c == '\'' && !inDouble) {            // toggle single-quote
                    inSingle = !inSingle;                       // flip
                } else if (c == '=' && !inSingle && !inDouble) { // '=' outside quotes
                    eqPos = i;                                  // record and break
                    break;                                      // stop loop
                }
            }

            if (eqPos == std::string::npos) {                   // if no '=' in this piece
                continue;                                       // skip invalid piece
            }

            std::string key = trim(piece.substr(0, eqPos));     // left of '=' is column name
            std::string val = cleanLiteral(piece.substr(eqPos + 1)); // right of '=' is cleaned value
            if (!key.empty()) {                                 // only store if key present
                out[key] = val;                                 // put into map
            }
        }
        return out;                                             // return assignments map
    }

    // --------------- CSV I/O ---------------

    static std::vector<std::vector<std::string>> readCSV(const std::string &path) {
        std::vector<std::vector<std::string>> rows;             // output rows
        std::ifstream file(path);                               // open file for reading
        if (!file.is_open()) {                                  // if cannot open
            return rows;                                        // return empty
        }
        std::string line;                                       // buffer for each line
        while (std::getline(file, line)) {                      // read each line
            std::vector<std::string> row;                       // current parsed row
            std::stringstream ss(line);                         // stream line for splitting
            std::string cell;                                   // cell buffer
            while (std::getline(ss, cell, ',')) {               // split by comma
                row.push_back(trim(cell));                      // push trimmed cell
            }
            if (!line.empty()) {                                // if line not empty
                if (line.back() == ',') {                       // if trailing comma
                    row.push_back("");                          // add empty cell
                }
            }
            rows.push_back(row);                                // push row
        }
        return rows;                                            // return all rows
    }

    static void writeCSV(const std::string &path, const std::vector<std::vector<std::string>> &rows) {
        std::ofstream file(path, std::ios::trunc);              // open file for writing (truncate)
        for (size_t r = 0; r < rows.size(); ++r) {              // iterate rows
            const auto &row = rows[r];                          // reference to current row
            for (size_t i = 0; i < row.size(); ++i) {           // iterate cells
                std::string cell = row[i];                      // copy cell text
                bool hasComma = (cell.find(',') != std::string::npos); // check for comma
                bool hasQuote = (cell.find('"') != std::string::npos); // check for quote
                bool needsQuotes = (hasComma || hasQuote);      // decide if quoting needed
                if (needsQuotes) {                              // if quoting needed
                    std::string esc;                            // buffer for escaped cell
                    for (size_t k = 0; k < cell.size(); ++k) {  // iterate characters
                        char c = cell[k];                       // current char
                        if (c == '"') {                         // if a quote
                            esc += "\"\"";                      // escape as double quote
                        } else {                                // otherwise
                            esc += c;                           // copy char
                        }
                    }
                    file << '"' << esc << '"';                  // write quoted, escaped cell
                } else {                                        // if no quoting needed
                    file << cell;                               // write raw cell
                }
                if (i + 1 < row.size()) {                       // if not last cell
                    file << ",";                                // write comma
                }
            }
            file << "\n";                                       // end of row
        }
    }

    std::vector<std::vector<std::string>> loadTable(const std::string &tableName) {
        fs::path p = dataRoot / (tableName + ".csv");           // build full path to table CSV
        return readCSV(p.string());                             // read and return rows
    }

    void saveTable(const std::string &tableName, const std::vector<std::vector<std::string>> &rows) {
        fs::path p = dataRoot / (tableName + ".csv");           // build full path to table CSV
        writeCSV(p.string(), rows);                             // write rows to disk
    }

    // --------------- Commands ---------------

    // CREATE TABLE name (col1, col2, ...)
    void createTable(const std::string &cmdRaw) {
        std::string cmd = stripTrailingSemicolon(cmdRaw);       // clean trailing semicolon
        size_t tblKw = findNoCase(cmd, "TABLE");                // find TABLE keyword
        size_t open = std::string::npos;                        // pos of '('
        size_t close = std::string::npos;                       // pos of ')'
        if (tblKw == std::string::npos) {                       // if TABLE not found
            std::cout << "Syntax error: missing keyword TABLE.\n"; // print message
            std::cout << "Usage: CREATE TABLE <name> (col1, col2, ...);\n"; // hint usage
            return;                                             // stop
        }
        open = cmd.find('(', tblKw);                            // find '(' after TABLE
        if (open == std::string::npos) {                        // if '(' missing
            std::cout << "Syntax error: column list required in parentheses.\n"; // message
            std::cout << "Example: CREATE TABLE amigos (id, name, active);\n";  // example
            return;                                             // stop
        }
        close = cmd.find(')', open + 1);                        // find ')'
        if (close == std::string::npos) {                       // if ')' missing
            std::cout << "Syntax error: missing closing ')'.\n"; // message
            return;                                             // stop
        }

        std::string between = trim(cmd.substr(tblKw + 5, open - (tblKw + 5))); // text between TABLE and '('
        std::string tableName = extractTableNameAfter("TABLE " + between, "TABLE"); // normalize extraction
        if (tableName.empty()) {                                // if no table name
            std::cout << "Syntax error: missing table name.\n"; // message
            std::cout << "Usage: CREATE TABLE <name> (col1, col2, ...);\n"; // hint
            return;                                             // stop
        }

        std::string colsRegion = cmd.substr(open, close - open + 1); // get "(col,...)" region
        std::vector<std::string> columns = parseParenList(colsRegion); // parse columns
        if (columns.empty()) {                                   // ensure we have columns
            std::cout << "No columns specified.\n";             // message
            std::cout << "Example: CREATE TABLE " << tableName << " (id, name, active);\n"; // hint
            return;                                             // stop
        }

        fs::path p = dataRoot / (tableName + ".csv");           // full path to CSV
        if (fs::exists(p)) {                                    // if file already exists
            std::cout << "Table \"" << tableName << "\" already exists.\n"; // message
            return;                                             // stop
        }

        std::vector<std::vector<std::string>> rows;             // rows buffer
        rows.push_back(columns);                                 // first row is header (column names)
        saveTable(tableName, rows);                              // write to disk
        std::cout << "Created table \"" << tableName << "\" with " 
                  << columns.size() << " column(s).\n";          // confirmation
    }

    // INSERT INTO name VALUES (v1, v2, ...)
    void insertIntoTable(const std::string &cmdRaw) {
        std::string cmd = stripTrailingSemicolon(cmdRaw);       // clean trailing semicolon
        std::string tableName = extractTableNameAfter(cmd, "INTO"); // extract table name
        if (tableName.empty()) {                                // if missing table name
            std::cout << "Syntax error: missing table name in INSERT.\n"; // message
            return;                                             // stop
        }

        size_t valuesPos = findNoCase(cmd, "VALUES");           // find VALUES keyword
        if (valuesPos == std::string::npos) {                   // if VALUES missing
            std::cout << "Syntax error: missing VALUES in INSERT.\n"; // message
            return;                                             // stop
        }
        std::string valuesPart = trim(cmd.substr(valuesPos + 6)); // slice after VALUES
        std::vector<std::string> values = parseParenList(valuesPart); // parse value list

        std::vector<std::vector<std::string>> rows = loadTable(tableName); // load table
        if (rows.empty()) {                                     // if table not found/empty
            std::cout << "Table \"" << tableName << "\" not found or empty. Create it first.\n"; // message
            return;                                             // stop
        }

        const std::vector<std::string> &header = rows[0];       // reference to header row
        if (values.size() != header.size()) {                   // ensure correct number of values
            std::cout << "Column count mismatch: expected " << header.size()
                      << " values, got " << values.size() << ".\n"; // message
            return;                                             // stop
        }

        rows.push_back(values);                                  // append new row
        saveTable(tableName, rows);                              // write back to disk
        std::cout << "Inserted 1 row into \"" << tableName << "\".\n"; // confirmation
    }

    // UPDATE name SET a=1, b="x" WHERE col = val
    void updateTable(const std::string &cmdRaw) {
        std::string cmd = stripTrailingSemicolon(cmdRaw);       // clean trailing semicolon
        std::string tableName = extractTableNameAfter(cmd, "UPDATE"); // extract table name
        if (tableName.empty()) {                                // if missing table name
            std::cout << "Syntax error: missing table name in UPDATE.\n"; // message
            return;                                             // stop
        }

        size_t setPos = findNoCase(cmd, "SET");                 // find SET
        if (setPos == std::string::npos) {                      // if SET missing
            std::cout << "Syntax error: missing SET in UPDATE.\n"; // message
            return;                                             // stop
        }

        std::string setAndRest = trim(cmd.substr(setPos));      // slice from SET to end
        size_t wherePos = findNoCase(setAndRest, "WHERE");      // find WHERE inside that
        std::string setPart;                                    // will hold "SET a=1, b=2"
        if (wherePos == std::string::npos) {                    // if no WHERE
            setPart = setAndRest;                               // entire tail is SET part
        } else {
            setPart = trim(setAndRest.substr(0, wherePos));     // up to WHERE is SET part
        }

        std::unordered_map<std::string, std::string> assigns = parseAssignments(setPart); // parse assignments
        std::pair<std::string, std::string> whereKV = parseWhereEquals(cmd); // parse WHERE (may be empty)
        std::string wcol = whereKV.first;                       // WHERE column
        std::string wval = whereKV.second;                      // WHERE value

        std::vector<std::vector<std::string>> rows = loadTable(tableName); // load table
        if (rows.empty()) {                                     // if table missing
            std::cout << "Table \"" << tableName << "\" not found or empty.\n"; // message
            return;                                             // stop
        }

        const std::vector<std::string> &header = rows[0];       // header row
        std::unordered_map<std::string, size_t> idx;            // column name -> index
        for (size_t i = 0; i < header.size(); ++i) {            // build index map
            idx[header[i]] = i;                                 // map header name to position
        }

        for (auto it = assigns.begin(); it != assigns.end(); ++it) { // validate assignment columns
            const std::string &colname = it->first;             // assignment column
            if (idx.count(colname) == 0) {                      // if column not in header
                std::cout << "Unknown column in SET: " << colname << "\n"; // message
                return;                                         // stop
            }
        }

        size_t whereIdx = static_cast<size_t>(-1);              // default: update all rows
        if (!wcol.empty()) {                                    // if WHERE given
            if (idx.count(wcol) == 0) {                         // ensure WHERE column exists
                std::cout << "Unknown column in WHERE: " << wcol << "\n"; // message
                return;                                         // stop
            }
            whereIdx = idx[wcol];                               // record WHERE column index
        }

        int updated = 0;                                        // counter of updated rows
        for (size_t r = 1; r < rows.size(); ++r) {              // iterate data rows
            bool match = true;                                  // assume match
            if (whereIdx != static_cast<size_t>(-1)) {          // if WHERE in effect
                if (rows[r][whereIdx] != wval) {                // compare cell with WHERE value
                    match = false;                              // no match
                }
            }
            if (match) {                                        // if row should be updated
                for (auto it = assigns.begin(); it != assigns.end(); ++it) { // iterate assignments
                    const std::string &colname = it->first;     // assignment column name
                    const std::string &newval = it->second;     // assignment new value
                    size_t cidx = idx[colname];                 // column index
                    rows[r][cidx] = newval;                     // assign new value
                }
                updated += 1;                                   // increment counter
            }
        }

        saveTable(tableName, rows);                             // write updated table
        std::cout << "Updated " << updated << " row(s) in \""   // print summary
                  << tableName << "\".\n";
    }

    // DELETE FROM name WHERE col = val
    void deleteFromTable(const std::string &cmdRaw) {
        std::string cmd = stripTrailingSemicolon(cmdRaw);       // clean trailing semicolon
        std::string tableName = extractTableNameAfter(cmd, "FROM"); // extract table name
        if (tableName.empty()) {                                // if missing table name
            std::cout << "Syntax error: missing table name in DELETE.\n"; // message
            return;                                             // stop
        }

        std::pair<std::string, std::string> whereKV = parseWhereEquals(cmd); // parse WHERE
        std::vector<std::vector<std::string>> rows = loadTable(tableName); // load table

        if (whereKV.first.empty()) {
            // No WHERE → ask user before deleting all rows
            char choice;
            std::cout << "WARNING: This will delete ALL records from table \"" 
                    << tableName << "\"!\n";
            std::cout << "Are you sure you want to continue? (Y/N): ";
            std::cin >> choice;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // <— flush leftover newline


            if (choice == 'y' || choice == 'Y') {
                // Keep only header (first row)
                std::vector<std::vector<std::string>> newRows;
                newRows.push_back(rows[0]); // keep header

                saveTable(tableName, newRows);
                std::cout << "All records deleted from \"" << tableName << "\".\n";
            } else {
                std::cout << "Operation cancelled.\n";
            }
            return;
        }

        if (rows.empty()) {                                     // if table missing
            std::cout << "Table \"" << tableName << "\" not found or empty.\n"; // message
            return;                                             // stop
        }

        const std::vector<std::string> &header = rows[0];       // header row
        size_t colIndex = static_cast<size_t>(-1);              // index of WHERE column
        for (size_t i = 0; i < header.size(); ++i) {            // search column by name
            if (header[i] == whereKV.first) {                   // if names match
                colIndex = i;                                   // record index
                break;                                          // stop loop
            }
        }
        if (colIndex == static_cast<size_t>(-1)) {              // if column not found
            std::cout << "Unknown column in WHERE: " << whereKV.first << "\n"; // message
            return;                                             // stop
        }

        std::vector<std::vector<std::string>> newRows;          // new table rows
        newRows.push_back(header);                              // keep header
        int deleted = 0;                                        // count deleted rows
        for (size_t i = 1; i < rows.size(); ++i) {              // iterate data rows
            if (rows[i][colIndex] == whereKV.second) {          // if row matches WHERE value
                deleted += 1;                                   // increment deleted
            } else {                                            // otherwise
                newRows.push_back(rows[i]);                     // keep the row
            }
        }

        saveTable(tableName, newRows);                          // write new table
        std::cout << "Deleted " << deleted << " row(s) from \"" // print summary
                  << tableName << "\".\n";
    }

    //SELECT <column name> FROM <name> WHERE col = val
    void selectTable(const std::string &cmdRaw) {
        std::string cmd = stripTrailingSemicolon(cmdRaw);
        std::string tableName = extractTableNameAfter(cmd, "FROM");

        if (tableName.empty()) {
            std::cout << "Syntax error: missing table name in DROP";
            return;
        }

        std::vector<std::vector<std::string>> rows = loadTable(tableName);

        if (rows.empty()) {
            std::cout << "Table \"" << tableName << "\" not found or empty.\n";
            return;
        }
    }
    
    // DROP TABLE <name>
    void dropTable(const std::string &cmdRaw) {
        std::string cmd = stripTrailingSemicolon(cmdRaw);
        std::string tableName = extractTableNameAfter(cmd, "TABLE");

        if (tableName.empty()) {
            std::cout << "Syntax error: missing table name in DROP";
            return;
        }

        std::vector<std::vector<std::string>> rows = loadTable(tableName);

        if (rows.empty()) {
            std::cout << "Table \"" << tableName << "\" not found.\n";
            return;
        }

        fs::path p = dataRoot / (tableName + ".csv");

        if (fs::remove(p)) {
            std::cout << "File '" << p << "' deleted successfully." << std::endl;
        } else {
            std::cout << "File '" << p << "' not found or could not be deleted." << std::endl;
        }
    }
    // ALTER TABLE name ADD/DROP columnName
    void alterTable(const std::string &cmdRaw) {                
        std::string cmd = stripTrailingSemicolon(cmdRaw);       // clean trailing semi colon
        std::string tableName = extractTableNameAfter(cmd, "TABLE"); // extract table name
        if (tableName.empty()) {                                // if table name is missing 
            std::cout << "Syntax error: missing table name in ALTER. \n"; // print out error message
            return;                                             // stop
        }

        std::vector<std::vector<std::string>> rows = loadTable(tableName); // load table
        
        if (rows.empty()) {                                     //
            std::cout << "Table \"" << tableName << "\" not found or empty.\n";
            return;
        }
        size_t addPos = findNoCase(cmd, "ADD");
        size_t dropPos = findNoCase(cmd, "DROP");

        if (addPos != std::string::npos && dropPos != std::string::npos) {
            std::cout << "Syntax error: cannot use both ADD and DROP in one command.\n";
            return;
        }

        if (addPos == std::string::npos && dropPos == std::string::npos) {
            std::cout << "Syntax error: expected ADD or DROP after table name.\n";
            return;
        }

        std::string colName;

        if (addPos != std::string::npos) {
            std::string colName = trim(cmd.substr(addPos + 3));
            if (colName.empty()) {
                std::cout << "Syntax error: missing column name for ADD.\n";
                return;
            }

            std::string newCol = stripTrailingSemicolon(colName);

            const std::vector<std::string> &header = rows[0];
            for (const auto &col : header) {
                if (col == newCol) {
                    std::cout << "Column \"" << newCol << "\" already exists.\n";
                    return;
                }
            }

            for (size_t r = 0;r < rows.size(); ++r) {
                if (r == 0) 
                    rows[r].push_back(newCol);
                else
                    rows[r].push_back("");
            }

            saveTable(tableName, rows);
            std::cout << "Added column \"" << newCol << "\" to table \"" << tableName << "\".\n";
        }
        else if (dropPos != std::string::npos) {
            std::string colName = trim(cmd.substr(dropPos + 4));
            if (colName.empty()) {
                std::cout << "Syntax error: mssing column name for DROP.\n";
                return;
            }

            std::string dropCol = stripTrailingSemicolon(colName);
            std::vector<std::string> &header = rows[0];

            size_t colIndex = static_cast<size_t>(-1);
            for (size_t i = 0; i < header.size(); ++i) {
                if (header[i] == dropCol) {
                    colIndex = i;
                    break;
                }
            }

            if (colIndex == static_cast<size_t>(-1)) {
                std::cout << "Unknown column: " << dropCol << "\n";
                return;
            }

            // Remove column from all rows
            for (auto &row : rows) {
                if (colIndex < row.size())
                    row.erase(row.begin() + colIndex);
            }

            saveTable(tableName, rows);
            std::cout << "Dropped column \"" << dropCol << "\" from table \"" << tableName << "\".\n";
    }
}

    // SHOW TABLE name (pretty prints header + rows)
    void showTable(const std::string &cmdRaw) {
        std::string tableName = extractTableNameAfter(cmdRaw, "TABLE"); // extract table name
        std::vector<std::vector<std::string>> rows = loadTable(tableName); // read table
        if (rows.empty()) {                                     // if missing/empty
            std::cout << "Table \"" << tableName << "\" not found or empty.\n"; // message
            return;                                             // stop
        }

        std::vector<size_t> widths(rows[0].size(), 0);          // per-column widths
        for (size_t r = 0; r < rows.size(); ++r) {              // iterate all rows
            const std::vector<std::string> &row = rows[r];      // current row
            for (size_t c = 0; c < row.size(); ++c) {           // iterate cells
                if (row[c].size() > widths[c]) {                // if wider than current max
                    widths[c] = row[c].size();                  // update max width
                }
            }
        }

        auto printSep = [&]() {                                 // helper to print separator line
            std::cout << '+';                                   // starting plus
            for (size_t w = 0; w < widths.size(); ++w) {        // each column
                std::cout << std::string(widths[w] + 2, '-')    // dashes for width + padding
                          << '+';                                // column separator
            }
            std::cout << "\n";                                  // newline
        };

        auto printRow = [&](const std::vector<std::string> &row) { // helper to print a row
            std::cout << '|';                                   // starting bar
            for (size_t c = 0; c < row.size(); ++c) {           // each cell
                std::cout << ' '                                // left padding
                          << std::left << std::setw((int)widths[c]) // align cell
                          << row[c]                              // cell text
                          << ' '                                // right padding
                          << '|';                               // column separator
            }
            std::cout << "\n";                                  // newline
        };

        printSep();                                             // top border
        printRow(rows[0]);                                      // header row
        printSep();                                             // header separator
        for (size_t r = 1; r < rows.size(); ++r) {              // each data row
            printRow(rows[r]);                                  // print row
        }
        printSep();                                             // bottom border
        std::cout << (rows.size() - 1) << " row(s).\n";         // count data rows
    }

    // SHOW PATH; (prints both the working directory and data directory)
    void showPath() {
        std::cout << "Current working directory: " 
                  << fs::current_path().string() << "\n";       // print CWD
        std::cout << "Data directory:           " 
                  << dataRoot.string() << "\n";                 // print resolved data dir
    }

public:
    // Constructor: resolve a stable data directory
    // - Prefers env MINISQL_DATA if set
    // - Otherwise, uses <exeDir>/data (next to the executable)
    MiniSQL(const fs::path &exePath) {
        const char *envDir = std::getenv("MINISQL_DATA");       // read env var pointer (may be null)
        if (envDir != nullptr) {                                // if env var provided
            fs::path envPath = fs::path(envDir);                // wrap as path
            dataRoot = fs::weakly_canonical(                    // make absolute, normalize
                fs::absolute(envPath)
            );
        } else {                                                // no env override
            fs::path exeAbs = fs::weakly_canonical(             // canonicalize executable path
                fs::absolute(exePath)
            );
            fs::path exeDir = exeAbs.parent_path();             // get directory of executable
            dataRoot = fs::weakly_canonical(                    // set to "<exeDir>/data"
                exeDir / "data"
            );
        }

        if (!fs::exists(dataRoot)) {                            // if directory missing
            fs::create_directories(dataRoot);                   // create it and parents
        }

        std::cout << "[MiniSQL] Using data directory: "         // print chosen data directory
                  << dataRoot.string() << "\n";                 // path as string

        std::cout << "[MiniSQL] Current working directory: "    // also print CWD for clarity
                  << fs::current_path().string() << "\n";       // path as string
    }

    // Main REPL loop
    void run() {
        std::cout << "Welcome to MiniSQL-CPP!\n";               // greeting
        std::cout << "Commands end with ';'. Supported: CREATE, INSERT, UPDATE, DELETE, SHOW, SHOW PATH, EXIT, ALTER\n\n"; // help

        std::string accum;                                      // buffer for multi-line statements
        while (true) {                                          // REPL loop
            std::cout << "sql> ";                               // prompt
            std::string line;                                   // input line
            if (!std::getline(std::cin, line)) {                // read line; if EOF
                break;                                          // exit loop
            }

            accum += line + "\n";                               // append to buffer
            if (accum.find(';') == std::string::npos) {         // if no ';' yet
                continue;                                       // keep reading
            }

            size_t semi = accum.find(';');                      // find first ';'
            std::string input = trim(accum.substr(0, semi + 1)); // one statement incl. ';'
            accum = trim(accum.substr(semi + 1));               // keep remainder (if any)

            if (input.empty()) {                                // ignore empty statement
                continue;                                       // next prompt
            }

            if (startsWithNoCase(input, "EXIT")) {              // user wants to exit
                std::cout << "Goodbye!\n";                      // farewell
                break;                                          // break loop
            } else if (startsWithNoCase(input, "CREATE TABLE")) {
                createTable(input);                             // handle CREATE
            } else if (startsWithNoCase(input, "INSERT INTO")) {
                insertIntoTable(input);                         // handle INSERT
            } else if (startsWithNoCase(input, "UPDATE")) {
                updateTable(input);                             // handle UPDATE
            } else if (startsWithNoCase(input, "DELETE FROM")) {
                deleteFromTable(input);                         // handle DELETE
            } else if (startsWithNoCase(input, "ALTER TABLE")) {
                alterTable(input);
            } else if (startsWithNoCase(input, "SHOW TABLE")) {
                showTable(input);                               // handle SHOW TABLE
            } else if (startsWithNoCase(input, "SHOW PATH")) {
                showPath();                                     // handle SHOW PATH
            } else if (startsWithNoCase(input, "DROP TABLE")) {
                dropTable(input);                               // handle DROP TABLE
            } else {
                std::cout << "Unknown command.\n";              // unknown input
            }
        }
    }
};

// -------------------- main --------------------
int main(int argc, char **argv) {
    fs::path exePath;                                           // path to the running executable (best effort)
    if (argc > 0) {                                             // if argv[0] exists
        exePath = fs::path(argv[0]);                            // use argv[0] as path (may be relative)
    } else {
        exePath = fs::current_path() / "MiniSQL";               // fallback: CWD + name
    }

    MiniSQL sql(exePath);                                       // construct with executable path
    sql.run();                                                  // start REPL
    return 0;                                                   // normal exit
}
