cmake_minimum_required(VERSION 3.10)
project(MyDatabaseProject CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Создание статической библиотеки СУБД (Требование 2)
add_library(mysqldb STATIC src/Database.cpp)

# Создание исполняемого файла, линкующего библиотеку
add_executable(MyDatabaseSystem src/main.cpp)
target_link_libraries(MyDatabaseSystem mysqldb)
