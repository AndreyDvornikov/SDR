cmake_minimum_required(VERSION 3.16)
project(PlutoSDR C CXX)

set(CMAKE_BUILD_TYPE Debug)

find_package(SoapySDR REQUIRED)

add_executable(sdr main.cpp)

if(TARGET SoapySDR::SoapySDR)
    target_link_libraries(sdr PRIVATE SoapySDR::SoapySDR)
else()
    target_link_libraries(sdr PRIVATE ${SoapySDR_LIBRARIES})
endif()
