cmake_minimum_required(VERSION 3.19)
project(lbp-simulator LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network SerialPort)

qt_standard_project_setup()

qt_add_executable(lbp-simulator
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h

    transport.h
    tcptransport.h tcptransport.cpp
    serialtransport.h serialtransport.cpp

    simview.h simview.cpp
    log.h log.cpp
	simworker.h simworker.cpp
    firmwaresim.h firmwaresim.cpp
    configuration.h configuration.cpp
    json.hpp
    movement.h movement.cpp
    simstate.h
    vec8.h
    filesystem.h filesystem.cpp
    simutils.h simutils.cpp
    transportwidget.h transportwidget.cpp
)

add_subdirectory(../lbp build)
add_subdirectory(../examples examples) # the example .lbpcor reader

target_link_libraries(lbp-simulator
    PRIVATE
        lbp
        lbpcor_example
        Qt::Core
        Qt::Widgets
        Qt6::Network
        Qt6::Widgets
        Qt6::SerialPort
)

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
