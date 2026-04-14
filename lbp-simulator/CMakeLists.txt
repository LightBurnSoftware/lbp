cmake_minimum_required(VERSION 3.19)
project(lbp-simulator LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(lbp-simulator
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h

    connection.h connection.cpp
    simview.h simview.cpp
    log.h log.cpp
    firmwaresim.h firmwaresim.cpp
    configuration.h configuration.cpp
    json.hpp
    movement.h movement.cpp

    simstate.h
    vec4.h vec4.cpp
    filesystem.h filesystem.cpp
    simutils.h
)

add_subdirectory(../lbp build)

target_link_libraries(lbp-simulator
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Network
        Qt6::Widgets
)
target_link_libraries(lbp-simulator PRIVATE lbp)

include(GNUInstallDirs)

install(TARGETS lbp-simulator
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET lbp-simulator
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
