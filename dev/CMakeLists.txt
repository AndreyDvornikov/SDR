cmake_minimum_required(VERSION 3.16)
project(PlutoSDR CXX)

set(CMAKE_BUILD_TYPE Debug)

# Опция для включения или отключения установки зависимостей
option(INSTALL_DEPS "Установить зависимости" ON)
option(PLUTO_TIMESTAMP "Build pluto with timestamp" ON)

# RF frontend driver
find_package(SoapySDR REQUIRED)

# Единый бинарь: режим задаётся параметром Tx/Rx.
add_executable(sdr main.cpp)
target_link_libraries(sdr ${SoapySDR_LIBRARIES})
