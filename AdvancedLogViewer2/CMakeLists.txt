cmake_minimum_required(VERSION 3.22)
project(alv2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort Network Concurrent Test)

# --- Core library (shared between app and tests) ---
set(ALV2_CORE_SOURCES
    src/core/LogStore.cpp
    src/core/FilterEngine.cpp
    src/core/LogDistributor.cpp
    src/core/LogReceiver.cpp
    src/core/LogFileWriter.cpp
    src/core/FilterWorker.cpp
    src/core/Profile.cpp
    src/core/ProfileManager.cpp
    src/core/Settings.cpp
    src/core/WatchdogTimer.cpp
    src/ui/LogViewModel.cpp
)

add_library(alv2_core STATIC ${ALV2_CORE_SOURCES})
target_link_libraries(alv2_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::Network
    Qt6::Concurrent
)
target_include_directories(alv2_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/test
)

# --- UI library (depends on Widgets; shared between app and UI tests) ---
set(ALV2_UI_SOURCES
    src/ui/TitleBar.cpp
    src/ui/LogDelegate.cpp
    src/ui/FilterBar.cpp
    src/ui/LogWindow.cpp
    src/ui/SettingsWindow.cpp
    src/ui/AppController.cpp
    src/test/TestServer.cpp
)

add_library(alv2_ui STATIC ${ALV2_UI_SOURCES})
target_link_libraries(alv2_ui PUBLIC alv2_core)

# --- Main executable ---
add_executable(alv2 src/main.cpp src/i18n/translations.qrc)
target_link_libraries(alv2 PRIVATE alv2_ui)

# --- Tests ---
enable_testing()

add_executable(test_logstore src/test/test_logstore.cpp)
target_link_libraries(test_logstore PRIVATE alv2_core Qt6::Test)
add_test(NAME LogStoreTest COMMAND test_logstore)

add_executable(test_filterengine src/test/test_filterengine.cpp)
target_link_libraries(test_filterengine PRIVATE alv2_core Qt6::Test)
add_test(NAME FilterEngineTest COMMAND test_filterengine)

add_executable(test_logdistributor src/test/test_logdistributor.cpp)
target_link_libraries(test_logdistributor PRIVATE alv2_core Qt6::Test)
add_test(NAME LogDistributorTest COMMAND test_logdistributor)

add_executable(test_logreceiver src/test/test_logreceiver.cpp)
target_link_libraries(test_logreceiver PRIVATE alv2_core Qt6::Test)
add_test(NAME LogReceiverTest COMMAND test_logreceiver)

add_executable(test_logfilewriter src/test/test_logfilewriter.cpp)
target_link_libraries(test_logfilewriter PRIVATE alv2_core Qt6::Test)
add_test(NAME LogFileWriterTest COMMAND test_logfilewriter)

add_executable(test_logviewmodel src/test/test_logviewmodel.cpp)
target_link_libraries(test_logviewmodel PRIVATE alv2_core Qt6::Test)
add_test(NAME LogViewModelTest COMMAND test_logviewmodel)

add_executable(test_profile src/test/test_profile.cpp)
target_link_libraries(test_profile PRIVATE alv2_core Qt6::Test)
add_test(NAME ProfileTest COMMAND test_profile)

add_executable(test_settingswindow src/test/test_settingswindow.cpp)
target_link_libraries(test_settingswindow PRIVATE alv2_ui Qt6::Test)
add_test(NAME SettingsWindowTest COMMAND test_settingswindow)
set_tests_properties(SettingsWindowTest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# --- Smoke test ---
add_test(
    NAME SmokeTest
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/smoke_test.sh $<TARGET_FILE:alv2>
)
set_tests_properties(SmokeTest PROPERTIES
    TIMEOUT 30
    LABELS "smoke"
)

# --- clang-tidy ---
set(ALL_SOURCES ${ALV2_SOURCES} ${ALV2_CORE_SOURCES})
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    add_custom_target(clang-tidy
        COMMAND ${CLANG_TIDY_EXE}
            -p ${CMAKE_BINARY_DIR}
            ${ALL_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy..."
    )
endif()
