cmake_minimum_required(VERSION 3.21)
project(DatyreDB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск пакетов, установленных через vcpkg
find_package(asio CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Создаем цели (Targets)
file(GLOB_RECURSE CORE_SRC "src/core/*.cpp")
add_library(datyredb_core STATIC ${CORE_SRC})
target_include_directories(datyredb_core PUBLIC include)
target_link_libraries(datyredb_core PRIVATE nlohmann_json::nlohmann_json)

file(GLOB_RECURSE NET_SRC "src/network/*.cpp")
add_library(datyredb_net STATIC ${NET_SRC})
target_link_libraries(datyredb_net 
    PUBLIC datyredb_core 
    PRIVATE asio::asio nlohmann_json::nlohmann_json Threads::Threads
)

add_executable(datyredb_server src/main.cpp)
target_link_libraries(datyredb_server PRIVATE datyredb_net)

if(WIN32)
    target_link_libraries(datyredb_server PRIVATE ws2_32 mswsock)
endif()
