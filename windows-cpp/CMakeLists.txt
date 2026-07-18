cmake_minimum_required(VERSION 3.20)
project(star_term_cpp VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg toolchain should be set via -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(OpenSSL REQUIRED)
find_package(Libssh2 REQUIRED)

qt_standard_project_setup()

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
    qt_add_executable(star_term_cpp WIN32
        ${SOURCES}
        ${HEADERS}
        ${APP_ICON_RESOURCE}
    )
else()
    qt_add_executable(star_term_cpp
        ${SOURCES}
        ${HEADERS}
    )
endif()

target_include_directories(star_term_cpp PRIVATE src)

target_link_libraries(star_term_cpp PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
    Libssh2::libssh2
)

if(WIN32)
    target_link_libraries(star_term_cpp PRIVATE ws2_32)
endif()

# Copy Qt DLLs on Windows for in-place running
if(WIN32)
    add_custom_command(TARGET star_term_cpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Build complete: $<TARGET_FILE:star_term_cpp>"
    )
endif()

add_compile_definitions(NOMINMAX)

