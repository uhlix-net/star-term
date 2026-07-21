cmake_minimum_required(VERSION 3.20)
project(star_term VERSION 0.4.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg toolchain should be set via -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(OpenSSL REQUIRED)
find_package(Libssh2 REQUIRED)

qt_standard_project_setup()

qt_add_resources(APP_RESOURCES resources.qrc)

set(SOURCES
    src/main.cpp
    src/config.cpp
    src/debug.cpp
    src/theme.cpp
    src/icons.cpp
    src/licensing.cpp
    src/vt100.cpp
    src/terminalwidget.cpp
    src/sshsession.cpp
    src/statsworker.cpp
    src/updatechecker.cpp
    src/rdppane.cpp
    src/sessionpane.cpp
    src/sidebar.cpp
    src/remotebrowser.cpp
    src/connectiondialog.cpp
    src/preferencesdialog.cpp
    src/licensedialog.cpp
    src/macrospanel.cpp
    src/mainwindow.cpp
    src/statusbar.cpp
)

set(HEADERS
    src/config.h
    src/debug.h
    src/colors.h
    src/theme.h
    src/icons.h
    src/licensing.h
    src/vt100.h
    src/terminalwidget.h
    src/sshsession.h
    src/statsworker.h
    src/updatechecker.h
    src/rdppane.h
    src/sessionpane.h
    src/sidebar.h
    src/remotebrowser.h
    src/connectiondialog.h
    src/preferencesdialog.h
    src/licensedialog.h
    src/macrospanel.h
    src/mainwindow.h
    src/statusbar.h
)

if(WIN32)
    # app.rc embeds app.ico into the .exe so Windows shows the correct icon.
    # Copy app.ico from the installer directory before building.
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/installer/app.rc")
    qt_add_executable(star_term WIN32
        ${SOURCES}
        ${HEADERS}
        ${APP_ICON_RESOURCE}
        ${APP_RESOURCES}
    )
else()
    qt_add_executable(star_term
        ${SOURCES}
        ${HEADERS}
        ${APP_RESOURCES}
    )
endif()

target_include_directories(star_term PRIVATE src)

target_link_libraries(star_term PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
    Libssh2::libssh2
)

if(WIN32)
    target_link_libraries(star_term PRIVATE ws2_32)
endif()

# After each Windows build: deploy Qt runtime DLLs and copy vcpkg DLLs next
# to the exe so the output directory is always self-contained and installer-ready.
if(WIN32)
    # Locate windeployqt from the same bin dir as qmake (works regardless of
    # how Qt6_DIR was specified on the cmake command line).
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)

    # 1. windeployqt — copies Qt DLLs, plugins, translations next to the exe.
    add_custom_command(TARGET star_term POST_BUILD
        COMMAND "${QT_BIN_DIR}/windeployqt.exe"
            "$<IF:$<CONFIG:Debug>,--debug,--release>"
            "$<TARGET_FILE:star_term>"
        COMMENT "Deploying Qt runtime DLLs..."
        VERBATIM
    )

    # 2. vcpkg runtime DLLs (libssh2, OpenSSL, zlib, etc.)
    add_custom_command(TARGET star_term POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/bin"
            "$<TARGET_FILE_DIR:star_term>"
        COMMENT "Copying vcpkg runtime DLLs..."
    )
endif()

add_compile_definitions(NOMINMAX)

