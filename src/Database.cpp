#include "Database.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>

// Вспомогательная функция для удаления пробелов и символов новой строки
std::string trim(std::string str) {
    str.erase(0, str.find_first_not_of(" \t\r\n"));
    str.erase(str.find_last_not_of(" \t\r\n") + 1);
    if (!str.empty() && str.back() == ';') str.pop_back();
    return str;
}

bool Table::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;
    
    columns.clear();
    rows.clear();
    
    std::string header_line;
    if (!std::getline(file, header_line)) return false;
    
    std::stringstream ss(header_line);
    std::string col_def;
    while (std::getline(ss, col_def, ',')) {
        size_t colon = col_def.find(':');
        if (colon != std::string::npos) {
            columns.push_back({col_def.substr(0, colon), col_def.substr(colon + 1)});
        }
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream row_ss(line);
        std::string val;
        Row row;
        size_t col_idx = 0;
        while (std::getline(row_ss, val, ',') && col_idx < columns.size()) {
            row.data[columns[col_idx].name] = val;
            col_idx++;
        }
        rows.push_back(row);
    }
    return true;
}

bool Table::saveToFile(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    
    for (size_t i = 0; i < columns.size(); ++i) {
        file << columns[i].name << ":" << columns[i].type;
        if (i < columns.size() - 1) file << ",";
    }
    file << "\n";
    
    for (const auto& row : rows) {
        for (size_t i = 0; i < columns.size(); ++i) {
            auto it = row.data.find(columns[i].name);
            file << (it != row.data.end() ? it->second : "NULL");
            if (i < columns.size() - 1) file << ",";
        }
        file << "\n";
    }
    return true;
}

DatabaseFacade::DatabaseFacade() : current_db("") {}

std::string DatabaseFacade::getDbPath() const {
    return "./" + current_db;
}

std::string DatabaseFacade::getTablePath(const std::string& table_name) const {
    return getDbPath() + "/" + table_name + ".tbl";
}

bool DatabaseFacade::tableExists(const std::string& table_name) {
    return access(getTablePath(table_name).c_str(), F_OK) == 0;
}

bool DatabaseFacade::loadTable(const std::string& table_name) {
    Table t;
    if (t.loadFromFile(getTablePath(table_name))) {
        t.name = table_name;
        loaded_tables[table_name] = t;
        return true;
    }
    return false;
}

bool DatabaseFacade::evaluateCondition(const Row& row, const std::string& col, const std::string& op, const std::string& val) {
    auto it = row.data.find(col);
    if (it == row.data.end()) return false;
    std::string cell = it->second;
    std::string clean_val = val;
    if (!clean_val.empty() && clean_val.front() == '\'') {
        clean_val = clean_val.substr(1, clean_val.size() - 2);
    }
    
    if (op == "=") return cell == clean_val;
    if (op == "!=") return cell != clean_val;
    
    try {
        double cell_num = std::stod(cell);
        double val_num = std::stod(clean_val);
        if (op == "<") return cell_num < val_num;
        if (op == ">") return cell_num > val_num;
        if (op == "<=") return cell_num <= val_num;
        if (op == ">=") return cell_num >= val_num;
    } catch (...) {
        if (op == "<") return cell < clean_val;
        if (op == ">") return cell > clean_val;
    }
    return false;
}

std::string DatabaseFacade::execute(const std::string& query_raw) {
    std::string query = trim(query_raw);
    
    // Регулярные выражения для парсинга DDL / DML
    std::regex create_db_regex(R"(CREATE\s+DATABASE\s+([A-Za-z0-9_]+))", std::regex_constants::icase);
    std::regex drop_db_regex(R"(DROP\s+DATABASE\s+([A-Za-z0-9_]+))", std::regex_constants::icase);
    std::regex drop_table_regex(R"(DROP\s+TABLE\s+([A-Za-z0-9_]+))", std::regex_constants::icase);
    std::regex create_table_regex(R"(CREATE\s+TABLE\s+([A-Za-z0-9_]+)\s*\((.*)\))", std::regex_constants::icase);
    std::regex insert_regex(R"(INSERT\s+INTO\s+([A-Za-z0-9_]+)\s*(?:\((.*?)\))?\s*VALUES\s*(.*))", std::regex_constants::icase);
    std::regex select_regex(R"(SELECT\s+(.+?)\s+FROM\s+([A-Za-z0-9_]+)(?:\s+WHERE\s+(.+?))?)", std::regex_constants::icase);
    std::regex delete_regex(R"(DELETE\s+FROM\s+([A-Za-z0-9_]+)(?:\s+WHERE\s+(.+?))?)", std::regex_constants::icase);
    std::regex update_regex(R"(UPDATE\s+([A-Za-z0-9_]+)\s+SET\s+(.+?)(?:\s+WHERE\s+(.+?))?)", std::regex_constants::icase);

    std::smatch match;

    // 1. CREATE DATABASE
    if (std::regex_match(query, match, create_db_regex)) {
        std::string db_name = match[1];
        if (mkdir(db_name.c_str(), 0777) == 0) {
            current_db = db_name;
            return "Database " + db_name + " created and selected.";
        }
        current_db = db_name; // Если уже существует, просто переключаемся
        return "Database " + db_name + " selected.";
    }

    // 2. DROP DATABASE
    if (std::regex_match(query, match, drop_db_regex)) {
        std::string db_name = match[1];
        std::string rmdir_cmd = "rm -rf " + db_name;
        std::system(rmdir_cmd.c_str());
        if (current_db == db_name) current_db = "";
        return "Database " + db_name + " dropped.";
    }

    if (current_db.empty()) {
        return "Error: No database selected. Run 'CREATE DATABASE db_name;' first.";
    }

    // 3. CREATE TABLE
    if (std::regex_match(query, match, create_table_regex)) {
        std::string table_name = match[1];
        std::string cols_raw = match[2];
        
        Table t;
        t.name = table_name;
        
        std::stringstream ss(cols_raw);
        std::string col_def;
        while (std::getline(ss, col_def, ',')) {
            col_def = trim(col_def);
            std::stringstream col_ss(col_def);
            std::string cname, ctype;
            col_ss >> cname >> ctype;
            t.columns.push_back({cname, ctype});
        }
        
        if (t.saveToFile(getTablePath(table_name))) {
            loaded_tables[table_name] = t;
            return "Table " + table_name + " created successfully.";
        }
        return "Error creating table.";
    }

    // 4. DROP TABLE
    if (std::regex_match(query, match, drop_table_regex)) {
        std::string table_name = match[1];
        if (tableExists(table_name)) {
            std::remove(getTablePath(table_name).c_str());
            loaded_tables.erase(table_name);
            return "Table " + table_name + " dropped.";
        }
        return "Error: Table not found.";
    }

    // 5. INSERT INTO
    if (std::regex_match(query, match, insert_regex)) {
        std::string table_name = match[1];
        std::string cols_spec = match[2];
        std::string values_raw = match[3];
        
        if (!tableExists(table_name) && loaded_tables.find(table_name) == loaded_tables.end()) {
            return "Error: Table " + table_name + " does not exist.";
        }
        loadTable(table_name);
        Table& t = loaded_tables[table_name];

        std::vector<std::string> target_cols;
        if (!cols_spec.empty()) {
            std::stringstream ss(cols_spec);
            std::string c;
            while (std::getline(ss, c, ',')) target_cols.push_back(trim(c));
        } else {
            for (const auto& col : t.columns) target_cols.push_back(col.name);
        }

        // Парсинг множественных строк VALUES ((v1, v2), (v3, v4))
        int affected_rows = 0;
        std::regex row_regex(R"(\((.*?)\))");
        auto words_begin = std::sregex_iterator(values_raw.begin(), values_raw.end(), row_regex);
        auto words_end = std::sregex_iterator();

        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch m = *i;
            std::string vals_str = m[1];
            std::stringstream ss(vals_str);
            std::string v;
            Row row;
            size_t val_idx = 0;
            
            while (std::getline(ss, v, ',')) {
                v = trim(v);
                if (val_idx < target_cols.size()) {
                    row.data[target_cols[val_idx]] = v;
                }
                val_idx++;
            }
            t.rows.push_back(row);
            affected_rows++;
        }
        t.saveToFile(getTablePath(table_name));
        return "Affected rows: " + std::to_string(affected_rows);
    }

    // 6. SELECT
    if (std::regex_match(query, match, select_regex)) {
        std::string projection = trim(match[1]);
        std::string table_name = match[2];
        std::string where_clause = match[3];

        if (!loadTable(table_name)) return "Error: Table " + table_name + " not found.";
        Table& t = loaded_tables[table_name];

        std::vector<std::string> print_cols;
        if (projection == "*") {
            for (const auto& c : t.columns) print_cols.push_back(c.name);
        } else {
            std::stringstream ss(projection);
            std::string c;
            while (std::getline(ss, c, ',')) print_cols.push_back(trim(c));
        }

        // Парсинг базового WHERE (col op val)
        std::string w_col = "", w_op = "", w_val = "";
        bool has_where = !where_clause.empty();
        if (has_where) {
            std::regex cond_regex(R"(([A-Za-z0-9_]+)\s*(=|!=|<|>|<=|>=)\s*(.+))");
            std::smatch cond_match;
            if (std::regex_match(where_clause, cond_match, cond_regex)) {
                w_col = cond_match[1];
                w_op = cond_match[2];
                w_val = trim(cond_match[3]);
            }
        }

        std::stringstream out;
        for (const auto& c : print_cols) out << c << "\t";
        out << "\n-------------------------------------\n";

        int count = 0;
        for (const auto& row : t.rows) {
            if (has_where && !evaluateCondition(row, w_col, w_op, w_val)) continue;
            for (const auto& c : print_cols) {
                auto it = row.data.find(c);
                out << (it != row.data.end() ? it->second : "NULL") << "\t";
            }
            out << "\n";
            count++;
        }
        out << "\n(" << count << " rows fetched)";
        return out.str();
    }

    // 7. DELETE FROM
    if (std::regex_match(query, match, delete_regex)) {
        std::string table_name = match[1];
        std::string where_clause = match[2];

        if (!loadTable(table_name)) return "Error: Table " + table_name + " not found.";
        Table& t = loaded_tables[table_name];

        std::string w_col = "", w_op = "", w_val = "";
        bool has_where = !where_clause.empty();
        if (has_where) {
            std::regex cond_regex(R"(([A-Za-z0-9_]+)\s*(=|!=|<|>|<=|>=)\s*(.+))");
            std::smatch cond_match;
            if (std::regex_match(where_clause, cond_match, cond_regex)) {
                w_col = cond_match[1];
                w_op = cond_match[2];
                w_val = trim(cond_match[3]);
            }
        }

        int affected_rows = 0;
        auto it = t.rows.begin();
        while (it != t.rows.end()) {
            if (!has_where || evaluateCondition(*it, w_col, w_op, w_val)) {
                it = t.rows.erase(it);
                affected_rows++;
            } else {
                ++it;
            }
        }
        t.saveToFile(getTablePath(table_name));
        return "Affected rows: " + std::to_string(affected_rows);
    }

    // 8. UPDATE
    if (std::regex_match(query, match, update_regex)) {
        std::string table_name = match[1];
        std::string set_clause = match[2];
        std::string where_clause = match[3];

        if (!loadTable(table_name)) return "Error: Table " + table_name + " not found.";
        Table& t = loaded_tables[table_name];

        // Парсинг SET col = val
        std::string s_col, s_val;
        std::regex set_regex(R"(([A-Za-z0-9_]+)\s*=\s*(.+))");
        std::smatch set_match;
        if (std::regex_match(set_clause, set_match, set_regex)) {
            s_col = set_match[1];
            s_val = trim(set_match[2]);
        }

        std::string w_col = "", w_op = "", w_val = "";
        bool has_where = !where_clause.empty();
        if (has_where) {
            std::regex cond_regex(R"(([A-Za-z0-9_]+)\s*(=|!=|<|>|<=|>=)\s*(.+))");
            std::smatch cond_match;
            if (std::regex_match(where_clause, cond_match, cond_regex)) {
                w_col = cond_match[1];
                w_op = cond_match[2];
                w_val = trim(cond_match[3]);
            }
        }

        int affected_rows = 0;
        for (auto& row : t.rows) {
            if (!has_where || evaluateCondition(row, w_col, w_op, w_val)) {
                row.data[s_col] = s_val;
                affected_rows++;
            }
        }
        t.saveToFile(getTablePath(table_name));
        return "Affected rows: " + std::to_string(affected_rows);
    }

    return "Error: Unknown or syntax-invalid command.";
}
