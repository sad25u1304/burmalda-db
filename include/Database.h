#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <map>

struct Column {
    std::string name;
    std::string type; // INT, FLOAT, BOOL, TEXT, VARCHAR
};

struct Row {
    std::map<std::string, std::string> data;
};

class Table {
public:
    std::string name;
    std::vector<Column> columns;
    std::vector<Row> rows;

    bool loadFromFile(const std::string& filepath);
    bool saveToFile(const std::string& filepath) const;
};

class DatabaseFacade {
private:
    std::string current_db;
    std::map<std::string, Table> loaded_tables;
    
    std::string getDbPath() const;
    std::string getTablePath(const std::string& table_name) const;
    bool tableExists(const std::string& table_name);
    bool loadTable(const std::string& table_name);
    bool evaluateCondition(const Row& row, const std::string& col, const std::string& op, const std::string& val);

public:
    DatabaseFacade();
    std::string execute(const std::string& query);
};

#endif
