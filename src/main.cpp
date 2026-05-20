#include "Database.h"
#include <iostream>
#include <string>

int main() {
    DatabaseFacade db;
    std::string query;
    
    std::cout << "=== Своя База DBMS MVP Client ===" << std::endl;
    std::cout << "Введите SQL-запрос (или 'exit' для выхода):" << std::endl;
    
    while (true) {
        std::cout << "SQL> ";
        std::getline(std::cin, query);
        if (query == "exit" || query == "quit") break;
        if (query.empty()) continue;
        
        std::string response = db.execute(query);
        std::cout << response << std::endl << std::endl;
    }
    return 0;
}
