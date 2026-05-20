#!/bin/bash

OS="$(uname)"
echo "Определена операционная система: $OS"

if [ "$OS" == "Linux" ]; then
    if [ -f /etc/debian_version ]; then
        echo "Установка зависимостей (g++, cmake)..."
        sudo apt update && sudo apt install -y build-essential cmake g++
    fi
fi

echo "Сборка проекта с помощью CMake..."
mkdir -p build
cd build
cmake ..
make
echo "Сборка завершена. Проверьте папку build/"
