# Поиск основной библиотеки DatyreDB
find_package(DatyreDB REQUIRED)

# Сборка примера локального использования
add_executable(example_embedded embedded_usage.cpp)
target_link_libraries(example_embedded DatyreDB::core)

# Сборка примера сетевого клиента
add_executable(example_client network_client.cpp)
target_link_libraries(example_client DatyreDB::client)