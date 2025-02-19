cmake_minimum_required(VERSION 3.22)
project(Scrapper)

set(BOOST_INCLUDE_LIBRARIES thread filesystem system program_options beast asio)
set(BOOST_ENABLE_CMAKE ON)
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
  Boost
  GIT_REPOSITORY https://github.com/boostorg/boost.git
  GIT_TAG boost-1.85.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Boost)

FetchContent_Declare(
  Boost
  GIT_REPOSITORY https://github.com/boostorg/boost.git
  GIT_TAG boost-1.85.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Boost)

FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.8.2
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        master 
)

FetchContent_MakeAvailable(tomlplusplus)

if(USE_SYSTEM_OPENSSL)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto SSL)
    if(OPENSSL_FOUND)
        include_directories(${OPENSSL_INCLUDE_DIR})
        message(STATUS "Using system OpenSSL")
    endif()
endif()

if(NOT USE_SYSTEM_OPENSSL OR NOT OPENSSL_FOUND)
    message(STATUS "Not using system OpenSSL, using built-in OpenSSL project instead.")
    FetchContent_Declare(
        OpenSSL
        GIT_REPOSITORY https://github.com/openssl/openssl
        GIT_TAG        "OpenSSL_1_1_1k"
    )

    FetchContent_MakeAvailable(OpenSSL)
endif()


set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

add_executable(scrapper main.cpp tradingSystem.cpp parser.cpp securitiesManager.cpp service.cpp webSocketConnection.cpp webSocketsManager.cpp)
target_link_libraries(scrapper PRIVATE Boost::filesystem Boost::program_options
	Boost::beast Boost::asio nlohmann_json::nlohmann_json spdlog OpenSSL::SSL OpenSSL::Crypto)

target_include_directories(scrapper PUBLIC ${tomlplusplus_SOURCE_DIR}/include)
