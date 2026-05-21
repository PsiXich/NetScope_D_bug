# ---------------------------------------------------------------------------
# netscope_core
# ---------------------------------------------------------------------------
add_library(netscope_core STATIC
    # --- core ---
    core/Message.cpp
    core/TcpClient.cpp
    core/TcpServer.cpp
    #core/WsClient.cpp
    #core/ConnectionManager.cpp

    # --- models ---
    #models/MessageLogModel.cpp
    #models/ConnectionListModel.cpp

    # --- utils ---
    #utils/Logger.cpp
)

target_include_directories(netscope_core
    PUBLIC
        # src/ в include path для всех кто линкуется к core:
        # в тестах и app #include "core/TcpClient.h"
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(netscope_core
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::WebSockets
        netscope_compile_options
)

# ---------------------------------------------------------------------------
# netscope_app — UI
# ---------------------------------------------------------------------------
set(APP_SOURCES
    main.cpp

    ui/MainWindow.cpp

    #ui/panels/TcpClientPanel.cpp
    #ui/panels/TcpServerPanel.cpp
    #ui/panels/WsClientPanel.cpp

    #ui/widgets/MessageLogView.cpp
    #ui/widgets/HexViewer.cpp

    #resources/resources.qrc   # раскомментировать когда появятся ресурсы
)

add_executable(netscope_app ${APP_SOURCES}
    core/message.h core/message.cpp
    core/tcpclient.h
    core/tcpclient.cpp
    core/tcpserver.h
    core/tcpserver.cpp)

target_link_libraries(netscope_app
    PRIVATE
        netscope_core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Gui
)

set_target_properties(netscope_app PROPERTIES
    OUTPUT_NAME "netscope"
    # Нужно для корректного bundle на macOS (не влияет на Linux/Windows)
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_VERSION     ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    # На Windows скрываем консоль в релизе.
    # В Debug консоль остаётся — удобно для qDebug().
    WIN32_EXECUTABLE $<CONFIG:Release>
)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS netscope_app
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)