# Project Roadmap: Simple SQL Clone in C++

## Phase 1: Core Design and Architecture

### Define System Scope
- **Supported DDL commands:**  
  - `CREATE TABLE`  
  - `DROP TABLE`  
  - `ALTER TABLE`  
  - `SHOW TABLES`
- **Supported DML commands:**  
  - `SELECT`  
  - `INSERT`  
  - `UPDATE`  
  - `DELETE`

### Data Storage Format
- Each table is stored as a **CSV file** in a `/data` directory.  
- A metadata file (e.g., `schema.json` or `tables.meta`) describes columns and data types.

---

## Phase 2: File Storage Layer

### Goals
Implement classes for **reading and writing CSV files safely**.

### Core Operations
- Load a table from CSV.  
- Write table data back to CSV after modifications.  
- Perform basic type validation and parsing.

---

## Phase 3: Schema and Metadata Management

### Objectives
Build a **TableManager** to:
- Store metadata (table name, columns, and types).  
- Create new tables (write empty CSV + update metadata).  
- Drop tables (delete CSV + update metadata).  

Metadata is stored in a separate text or JSON file, such as `tables.meta`.

---

## Phase 4: Command Parsing

### Tasks
Create a **SQL parser** that:
- Reads user input (e.g., `INSERT INTO students VALUES (...)`).  
- Tokenizes it (breaks into keywords, identifiers, and values).  
- Builds a simple structure or enum representing the command type.

---

## Phase 5: Command Execution Layer

### Components
Implement **executor classes** for each command type:
- **DDL Executor:** handles `CREATE`, `DROP`, `ALTER`, `SHOW`.  
- **DML Executor:** handles `SELECT`, `INSERT`, `UPDATE`, `DELETE`.

Each executor interacts with the **File I/O layer** and **TableManager** to perform operations.

---

## Phase 6: Query Engine (Basic Logic)

### Features
- **SELECT:** Support filtering with `WHERE` clauses and basic comparison operators.  
- **UPDATE** and **DELETE:** Apply condition checks on loaded CSV rows.  
- Keep all processing **in-memory per query**, and write back changes at the end.

---

## Phase 7: User Interface (CLI)

### Build a Simple REPL (Read-Eval-Print Loop)
- Accepts user commands line by line.  
- Passes them to the parser → executor → prints results.  
- Handles interactive errors and invalid commands gracefully.

---

## Phase 8: Testing and Error Handling

### Testing
- Run functional tests with real commands:  
  - Create tables, insert data, and perform queries.  
- Validate correct CSV updates and metadata consistency.

### Error Handling
- Add syntax validation and user-friendly error messages.  
- Handle invalid column names, data types, and file errors safely.

---

## Example CLI Session

Below is an example of how the program might look when running in the terminal:
```
Welcome to MiniSQL-CPP!
Type "exit;" to quit.

sql> SHOW TABLES;
No tables found.

sql> CREATE TABLE students (id INT, name TEXT, grade FLOAT);
Table "students" created successfully.

sql> INSERT INTO students VALUES (1, 'Alice', 3.9);
1 row inserted.

sql> INSERT INTO students VALUES (2, 'Bob', 3.5);
1 row inserted.

sql> SELECT * FROM students;
+----+-------+-------+
| id | name | grade |
+----+-------+-------+
| 1 | Alice | 3.9 |
| 2 | Bob | 3.5 |
+----+-------+-------+
2 rows returned.

sql> UPDATE students SET grade = 4.0 WHERE name = 'Alice';
1 row updated.

sql> DELETE FROM students WHERE id = 2;
1 row deleted.

sql> SELECT * FROM students;
+----+-------+-------+
| id | name | grade |
+----+-------+-------+
| 1 | Alice | 4.0 |
+----+-------+-------+
1 row returned.

sql> DROP TABLE students;
Table "students" dropped.

sql> exit;
Goodbye!
```
---
