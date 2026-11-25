cmake_minimum_required(VERSION 3.21)

project(BuildORM LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W4)
endif()

find_package(SqliteOrm CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# Бібліотека: уся логіка ЛР8 (ORM, Repository) + ЛР10 (State, Strategy, Observer, Facade)
add_library(buildcore STATIC
    src/Database.cpp
    src/ProjectRepository.cpp
    src/BuildDao.cpp
    src/BuildState.cpp
    src/DurationStrategy.cpp
    src/BuildPipelineFacade.cpp
)
target_include_directories(buildcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(buildcore PUBLIC sqlite_orm::sqlite_orm unofficial::sqlite3::sqlite3)

add_executable(BuildORM src/main.cpp)
target_link_libraries(BuildORM PRIVATE buildcore)

# Модульні тести
option(BUILD_TESTING "Збирати модульні тести" ON)
if(BUILD_TESTING)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_executable(BuildTests tests/BuildTests.cpp)
    target_link_libraries(BuildTests PRIVATE buildcore GTest::gtest GTest::gtest_main)
    add_test(NAME BuildTests COMMAND BuildTests)
endif()
