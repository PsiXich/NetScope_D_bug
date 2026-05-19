# ---------------------------------------------------------------------------
# функции для QTest
# использование netscope_add_test(NAME tst_TcpClient SOURCES tst_TcpClient.cpp)
# ---------------------------------------------------------------------------
function(netscope_add_test)
    cmake_parse_arguments(ARG "" "NAME" "SOURCES" ${ARGN})

    add_executable(${ARG_NAME} ${ARG_SOURCES})

    target_link_libraries(${ARG_NAME}
        PRIVATE
            netscope_core
            Qt${QT_VERSION_MAJOR}::Test
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Network
            Qt${QT_VERSION_MAJOR}::WebSockets
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
    NAME    tst_TcpClient
    SOURCES tst_TcpClient.cpp
)