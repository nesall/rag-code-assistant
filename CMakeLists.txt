cmake_minimum_required(VERSION 3.14)
project(embeddings_cpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent module
include(FetchContent)

# Fetch cpp-httplib
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
)

# Fetch hnswlib
FetchContent_Declare(
    hnswlib
    GIT_REPOSITORY https://github.com/nmslib/hnswlib.git
    GIT_TAG v0.8.0
)

# Bundle SQLite3 directly
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450300.zip
)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)

# Make dependencies available
FetchContent_MakeAvailable(httplib hnswlib sqlite3 nlohmann_json)

# Create SQLite3 library
add_library(sqlite3_lib ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3_lib PUBLIC ${sqlite3_SOURCE_DIR})

# Create executable
add_executable(${PROJECT_NAME} 
  settings.json
  main.cpp
  include/tokenizer.h
  include/settings.h
  include/chunker.h
  include/embedder.h
  src/tokenizer.cpp
  src/settings.cpp
  src/chunker.cpp
  src/embedder.cpp
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
    httplib::httplib
    sqlite3_lib
    nlohmann_json::nlohmann_json
)

# Include directories
target_include_directories(${PROJECT_NAME} 
    PRIVATE 
    include/
    ${hnswlib_SOURCE_DIR}
)

# Enable threading (required by hnswlib)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

# Platform-specific settings
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 wsock32)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()