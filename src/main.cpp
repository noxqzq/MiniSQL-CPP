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
//   DROP TABLE <name>;
//   SELECT <col name> FROM <name> WHERE <col name> = value;
//   SHOW TABLE <name>;
//   SHOW PATH;    // prints CWD and resolved data directory
//   EXIT;
//
// Parsing notes:
// - Values may be 'single' or "double" quoted; commas inside quotes are supported.
// - This is intentionally simple; no type system or schema enforcement beyond column count.

#include "MiniSQL.hpp"
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char **argv) {
    fs::path exePath = (argc>0? fs::path(argv[0]) : fs::current_path()/"MiniSQL");
    MiniSQL sql(exePath);
    sql.run();
    return 0;
}