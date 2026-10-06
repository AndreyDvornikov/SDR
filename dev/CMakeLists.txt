cmake_minimum_required(VERSION 3.16)
project(PlutoSDR CXX)

set(CMAKE_BUILD_TYPE Debug)

# Опция для включения или отключения установки зависимостей
option(INSTALL_DEPS "Установить зависимости" ON)
option(PLUTO_TIMESTAMP "Build pluto with timestamp" ON)

# RF frontend driver
find_package(SoapySDR REQUIRED)

# TX бинарь
add_executable(tx tx.cpp)
target_link_libraries(tx ${SoapySDR_LIBRARIES})

# RX бинарь
add_executable(rx rx.cpp)
target_link_libraries(rx ${SoapySDR_LIBRARIES})
