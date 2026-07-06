cmake_minimum_required(VERSION 3.10)
project(Server)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_path(BOOST_INCLUDE_DIR
        NAMES boost/beast.hpp
        PATHS /opt/homebrew/include
        /opt/homebrew/opt/boost/include
        /usr/local/include
        /usr/include
        REQUIRED
)

# Экспорт команд для Zed (чтобы работало автодополнение)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(Server
    source/main.cpp
    source/CssWork.cpp
    source/Server.cpp
    include/Server.h
    source/ClientSession.cpp
    include/ClientSession.h
    include/GroupData.h
    source/ImitatorFar.cpp
    include/ImitatorFar.h
        include/Logger.h
)

target_include_directories(Server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(
        Server PRIVATE
        ${BOOST_INCLUDE_DIR}
        src
)

target_link_libraries(Server
        ${Boost_LIBRARIES}
)

# Говорим Beast использовать std::error_code вместо boost::system::error_code
target_compile_definitions(Server PRIVATE
        BOOST_BEAST_USE_STD_ERIC=1
        BOOST_ASIO_USE_STD_ERIC=1
)
