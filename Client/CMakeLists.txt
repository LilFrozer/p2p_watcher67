cmake_minimum_required(VERSION 3.19)
project(Client LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(Client
    WIN32 MACOSX_BUNDLE
    source/mainclient.cpp
    source/mainclient.ui
    source/connection.cpp
    source/mainclient.cpp
    source/main.cpp
    source/qcustomplot.cpp
    include/mainclient.h
    include/connection.h
    include/qcustomplot.h
    include/GroupData.h
)

target_link_libraries(Client
    PRIVATE
        Qt::Core
        Qt::Widgets
        # Qt::PrintSupport
        # Qt::Multimedia
)

target_include_directories(Client
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${BOOST_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_definitions(Client
    PRIVATE
    QT_NO_PRINTER
    BOOST_BEAST_USE_STD_ERIC=1
    BOOST_ASIO_USE_STD_ERIC=1
)

include(GNUInstallDirs)

install(TARGETS Client
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Client
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
