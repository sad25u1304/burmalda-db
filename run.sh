#!/bin/bash

HOST="127.0.0.1"
PORT="8080"

while getopts "h:p:" opt; do
  case $opt in
    h) HOST=$OPTARG ;;
    p) PORT=$OPTARG ;;
  esac
done

if [ ! -f "build/MyDatabaseSystem" ]; then
    echo "Исполняемый файл не найден. Запуск сборки..."
    chmod +x build.sh
    ./build.sh
fi

echo "Запуск СУБД на $HOST:$PORT..."
./build/MyDatabaseSystem
