# ---------------------------------------------------------------------------
# Для классов с Q_OBJECT объявленных внутри .cpp файла (не .h)
# правильный подход — AUTOMOC + #include "filename.moc" в конце .cpp.
#
# qt_wrap_cpp генерирует moc_header.cpp для классов из .h файлов.
# Для классов из .cpp — он генерирует неправильный вывод → vtable errors.
#
# Убеждаемся что AUTOMOC включён для каждого тест-таргета явно.
# ---------------------------------------------------------------------------
function(netscope_add_test)
    cmake_parse_arguments(ARG "" "NAME" "SOURCES" ${ARGN})

    add_executable(${ARG_NAME} ${ARG_SOURCES})

    # Явное включение AUTOMOC на таргете — не полагаемся только на глобальный
    # CMAKE_AUTOMOC из корневого CMakeLists.txt
    set_target_properties(${ARG_NAME} PROPERTIES
        AUTOMOC ON
    )

    target_link_libraries(${ARG_NAME}
        PRIVATE
            netscope_core
            Qt${QT_VERSION_MAJOR}::Test
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Gui
            Qt${QT_VERSION_MAJOR}::Network
            Qt${QT_VERSION_MAJOR}::WebSockets
            netscope_compile_options
    )

    target_include_directories(${ARG_NAME}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_test(
        NAME    ${ARG_NAME}
        COMMAND ${ARG_NAME} -v
    )

    set_tests_properties(${ARG_NAME} PROPERTIES TIMEOUT 30)

endfunction()

netscope_add_test(
    NAME    tst_tcpclient
    SOURCES tst_tcpclient.cpp
)

netscope_add_test(
    NAME    tst_messagelogmodel
    SOURCES tst_messagelogmodel.cpp
)

netscope_add_test(
    NAME    tst_connectionmanager
    SOURCES tst_connectionmanager.cpp
)

netscope_add_test(
    NAME    tst_wsclient
    SOURCES tst_wsclient.cpp
)

netscope_add_test(
    NAME    tst_tcpserver
    SOURCES tst_tcpserver.cpp
)

netscope_add_test(
    NAME    tst_wsserver
    SOURCES tst_wsserver.cpp
)

netscope_add_test(
    NAME    tst_udpendpoint
    SOURCES tst_udpendpoint.cpp
)
