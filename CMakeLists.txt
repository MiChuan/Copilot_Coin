cmake_minimum_required(VERSION 3.16)
project(Copilot_Coin VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/utf-8)
endif()

# Linux needs pthread for std::thread / std::mutex
if(UNIX AND NOT APPLE)
  find_package(Threads REQUIRED)
endif()

# FetchContent: nlohmann-json (header-only library)
include(FetchContent)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(${PROJECT_NAME}
  src/main.cpp
  src/binance_http.cpp
  src/indicators.cpp
  src/strategy.cpp
  src/executor.cpp
  src/backtest.cpp
  src/util.cpp
  src/mock_data_generator.cpp
  src/csv_kline_loader.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
  CURL::libcurl
  OpenSSL::Crypto
  nlohmann_json::nlohmann_json
)

# Link pthread on Linux
if(UNIX AND NOT APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE -DSIMULATION=1)

# Test executable for CSV loading
add_executable(test_csv
  src/test_csv.cpp
  src/csv_kline_loader.cpp
)

target_include_directories(test_csv PRIVATE src)

target_link_libraries(test_csv PRIVATE
  nlohmann_json::nlohmann_json
)

# Test executable for Demo API
add_executable(test_demo_api
  src/test_demo_api.cpp
  src/binance_http.cpp
  src/util.cpp
  src/mock_data_generator.cpp
  src/csv_kline_loader.cpp
)

target_include_directories(test_demo_api PRIVATE src)

target_link_libraries(test_demo_api PRIVATE
  CURL::libcurl
  OpenSSL::Crypto
  nlohmann_json::nlohmann_json
)

# Link pthread on Linux for test_demo_api
if(UNIX AND NOT APPLE)
  target_link_libraries(test_demo_api PRIVATE Threads::Threads)
endif()

target_compile_definitions(test_demo_api PRIVATE -DSIMULATION=1)