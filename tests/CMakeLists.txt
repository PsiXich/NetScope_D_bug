# ---------------------------------------------------------------------------
# функции для QTest
# использование netscope_add_test(NAME tst_TcpClient SOURCES tst_TcpClient.cpp)
# ---------------------------------------------------------------------------
function(netscope_add_test)
    cmake_parse_arguments(ARG "" "NAME" "SOURCES" ${ARGN})

    add_executable(${ARG_NAME} ${ARG_SOURCES})

    # Явно включаем AUTOMOC для каждого тест-таргета
    # Глобального CMAKE_AUTOMOC недостаточно когда Q_OBJECT объявлен
    # в .cpp файле — нужен explicit property чтобы CMake гарантированно
    # запустил moc и сгенерировал .moc файл до компиляции
    set_target_properties(${ARG_NAME} PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON
    )

    target_link_libraries(${ARG_NAME}
        PRIVATE
            netscope_core
            Qt${QT_VERSION_MAJOR}::Test
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Network
            Qt${QT_VERSION_MAJOR}::WebSockets
            Qt${QT_VERSION_MAJOR}::Gui
            netscope_compile_options
    )

    target_include_directories(${ARG_NAME}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    # COMMAND бинарник с флагом -v (verbose)
    add_test(
        NAME    ${ARG_NAME}
        COMMAND ${ARG_NAME} -v
    )

    set_tests_properties(${ARG_NAME} PROPERTIES TIMEOUT 30)

endfunction()
# ---------------------------------------------------------------------------
# Регистрация тестов
# ---------------------------------------------------------------------------

netscope_add_test(
    NAME    tst_tcpclient
    SOURCES tst_tcpclient.cpp
)

#netscope_add_test(
    #NAME    tst_TcpServer
    #SOURCES tst_TcpServer.cpp
#)

#netscope_add_test(
    #NAME    tst_WsClient
    #SOURCES tst_WsClient.cpp
#)

netscope_add_test(
    NAME    tst_MessageLogModel
    SOURCES tst_MessageLogModel.cpp
)

#netscope_add_test(
    #NAME    tst_ConnectionManager
    #SOURCES tst_ConnectionManager.cpp
#)