cmake_minimum_required(VERSION 3.26)
project(Database_Server)

set(CMAKE_CXX_STANDARD 23)

add_executable(Database_Server main.cpp)

target_link_libraries(Database_Server PRIVATE ws2_32 mswsock)

find_package(Boost REQUIRED COMPONENTS system)
find_package(SQLite3 REQUIRED)
find_package(PythonLibs REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Widgets Sql)

target_include_directories(Database_Server PRIVATE
        ${Boost_INCLUDE_DIRS}
        ${SQLite3_INCLUDE_DIRS}

)

target_link_libraries(Database_Server PRIVATE
        ${Boost_LIBRARIES}
        ${SQLite3_LIBRARIES}
        ${PYTHON_LIBRARIES}
        Qt5::Widgets
        Qt5::Sql
)