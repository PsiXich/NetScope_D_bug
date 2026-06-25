cmake_minimum_required(VERSION 3.16)

project(NetScopeDebug
    VERSION 0.1.0
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# C++ standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Qt автогенерация
# ---------------------------------------------------------------------------
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---------------------------------------------------------------------------
# Пути артефактов сборки
# ---------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---------------------------------------------------------------------------
# Поиск Qt — поддерживаем и Qt5 и Qt6
# ---------------------------------------------------------------------------
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    WebSockets
    Mqtt
    Test
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    WebSockets
    Mqtt
    Test
)

message(STATUS "Building with Qt ${Qt${QT_VERSION_MAJOR}Core_VERSION}")

# ---------------------------------------------------------------------------
# Интерфейсный таргет с флагами компилятора
# ---------------------------------------------------------------------------
add_library(netscope_compile_options INTERFACE)

target_compile_options(netscope_compile_options INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /permissive-
    >
)

target_compile_definitions(netscope_compile_options INTERFACE
    NETSCOPE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    NETSCOPE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    NETSCOPE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    NETSCOPE_VERSION_STR="${PROJECT_VERSION}"
)

# ---------------------------------------------------------------------------
# Подпроекты
# ---------------------------------------------------------------------------
enable_testing()

add_subdirectory(src)
add_subdirectory(tests)