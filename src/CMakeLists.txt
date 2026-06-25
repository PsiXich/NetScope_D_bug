# ---------------------------------------------------------------------------
# netscope_core — вся логика без UI
# Тесты линкуются именно к этой библиотеке.
# netscope_app линкуется к ней же — не дублируем исходники.
# ---------------------------------------------------------------------------
add_library(netscope_core STATIC
    # --- core ---
    core/Message.cpp
    core/TcpClient.cpp
    core/TcpServer.cpp
    core/WsClient.cpp
    core/ConnectionManager.cpp
    core/WsServer.cpp
    core/UdpEndpoint.cpp

    # --- models ---
    models/MessageLogModel.cpp
    models/ConnectionListModel.cpp

    # --- utils ---
    utils/Logger.cpp
)

target_include_directories(netscope_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(netscope_core
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::WebSockets
        Qt${QT_VERSION_MAJOR}::Mqtt
        netscope_compile_options
)

# ---------------------------------------------------------------------------
# netscope_app — только UI, вся логика через netscope_core
# ---------------------------------------------------------------------------
set(APP_SOURCES
    main.cpp

    ui/MainWindow.cpp

    ui/panels/TcpClientPanel.cpp
    ui/panels/TcpServerPanel.cpp
    ui/panels/WsClientPanel.cpp
    ui/panels/WsServerPanel.cpp
    ui/panels/UdpEndpointPanel.cpp

    ui/widgets/MessageLogView.cpp
    ui/widgets/HexViewer.cpp
)

add_executable(netscope_app ${APP_SOURCES})

target_link_libraries(netscope_app
    PRIVATE
        netscope_core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Gui
)

set_target_properties(netscope_app PROPERTIES
    OUTPUT_NAME "netscope"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    WIN32_EXECUTABLE $<CONFIG:Release>
)

include(GNUInstallDirs)
install(TARGETS netscope_app
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)