cmake_minimum_required(VERSION 3.16)
project(PlutoSDR CXX)

set(CMAKE_BUILD_TYPE Debug)

# Опция для включения или отключения установки зависимостей
option(INSTALL_DEPS "Установить зависимости" ON)
option(PLUTO_TIMESTAMP "Build pluto with timestamp" ON)

# RF frontend driver
find_package(SoapySDR REQUIRED)


add_executable(main main.cpp)
target_link_libraries(main ${SoapySDR_LIBRARIES})

add_executable(loopback main_lb.cpp)
target_link_libraries(loopback ${SoapySDR_LIBRARIES})

add_executable(tr transmitter.cpp)
target_link_libraries(tr ${SoapySDR_LIBRARIES})
