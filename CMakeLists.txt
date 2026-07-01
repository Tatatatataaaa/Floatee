cmake_minimum_required(VERSION 3.16)
project(Floatee VERSION 1.5 LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Platform-specific sources
if(WIN32)
    set(PLATFORM_SOURCES
        src/platform/platformwindowinfo_win.cpp
    )
elseif(APPLE)
    set(PLATFORM_SOURCES
        src/platform/platformwindowinfo_mac.mm
    )
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(APPKIT_LIBRARY AppKit REQUIRED)
    find_library(APP_SERVICES ApplicationServices REQUIRED)
elseif(UNIX)
    find_package(X11 REQUIRED)
    set(PLATFORM_SOURCES
        src/platform/platformwindowinfo_x11.cpp
    )
endif()

set(SOURCES
    src/main.cpp
    src/core/teedrawer.cpp
    src/core/jsonopt.cpp
    src/ui/floatee.cpp
    src/ui/teeyes.cpp
    src/ui/windowsidehide.cpp
    ${PLATFORM_SOURCES}
)

set(HEADERS
    src/core/teedrawer.h
    src/core/jsonopt.h
    src/ui/floatee.h
    src/ui/teeyes.h
    src/ui/windowsidehide.h
    src/platform/platformwindowinfo.h
)

set(UI_FORMS
    src/ui/floatee.ui
    src/ui/teeyes.ui
)

qt_add_executable(Floatee
    ${SOURCES}
    ${HEADERS}
    ${UI_FORMS}
    Floatee.qrc
)

if(APPLE)
    set_source_files_properties(src/ui/windowsidehide.cpp
        PROPERTIES LANGUAGE OBJCXX)
endif()

target_include_directories(Floatee PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

set_target_properties(Floatee PROPERTIES WIN32_EXECUTABLE TRUE)

if(APPLE)
    set(EXECUTABLE_NAME "Floatee")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
        @ONLY
    )
    set_target_properties(Floatee PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.floatee.app"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_ICON_FILE "Floatee.icns"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    )
    set_source_files_properties(assets/Floatee.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(Floatee PRIVATE assets/Floatee.icns)
endif()

target_link_libraries(Floatee PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# ── Post-build: copy MinGW runtime DLLs (windeployqt doesn't handle these) ──
if(MINGW)
    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(MINGW_DLLS
        libstdc++-6.dll
        libgcc_s_seh-1.dll
        libwinpthread-1.dll
    )
    foreach(dll ${MINGW_DLLS})
        add_custom_command(TARGET Floatee POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN_DIR}/${dll}"
                "$<TARGET_FILE_DIR:Floatee>"
            COMMENT "Copying ${dll} to build dir"
        )
    endforeach()
endif()

if(WIN32)
    target_link_libraries(Floatee PRIVATE winmm)
elseif(APPLE)
    target_link_libraries(Floatee PRIVATE ${COCOA_LIBRARY} ${APPKIT_LIBRARY} ${APP_SERVICES})
elseif(UNIX)
    target_link_libraries(Floatee PRIVATE X11::X11)
endif()

# Platform defines
if(WIN32)
    target_compile_definitions(Floatee PRIVATE PLATFORM_WINDOWS)
elseif(APPLE)
    target_compile_definitions(Floatee PRIVATE PLATFORM_MACOS)
elseif(UNIX)
    target_compile_definitions(Floatee PRIVATE PLATFORM_LINUX)
endif()
