cmake_minimum_required(VERSION 3.16)
project(Copilot_Coin VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  # Use a newer compatible nlohmann/json tag
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

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
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
  CURL::libcurl
  OpenSSL::Crypto
  nlohmann_json::nlohmann_json
)

# Optional: compile definitions to toggle testnet/simulate
target_compile_definitions(${PROJECT_NAME} PRIVATE -DSIMULATION=1)
