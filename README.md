# Project Roadmap: Simple SQL Clone in C++

## Core Design and Architecture

### Define System Scope
- **Supported DDL commands:**  
  - `CREATE TABLE`  
  - `DROP TABLE`  
  - `ALTER TABLE`  
  - `SHOW TABLE`
- **Supported DML commands:**  
  - `SELECT`  
  - `INSERT`  
  - `UPDATE`  
  - `DELETE`

- **Extra Commands:**
  - `SHOW PATH`
  - `EXIT`

### Data Storage Format
- Each table is stored as a **CSV file** in a `/data` directory.  
- A metadata file (e.g., `schema.json` or `tables.meta`) describes columns and data types.

## File Storage Layer

### Goals
Implement classes for **reading and writing CSV files safely**.

### Core Operations
- Load a table from CSV.  
- Write table data back to CSV after modifications.  
- Perform basic type validation and parsing.


### Testing
- Run functional tests with real commands:  
  - Create tables, insert data, and perform queries.  
- Validate correct CSV updates and metadata consistency.

### Error Handling
- Add syntax validation and user-friendly error messages.  
- Handle invalid column names, data types, and file errors safely.


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
