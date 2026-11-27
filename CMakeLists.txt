cmake_minimum_required(VERSION 3.21)

project(NodIris VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

# Sources and translations are UTF-8; MSVC assumes the local code page unless told.
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# Only the three base modules. Every extra Qt module is more RAM at startup
# and one more library to ship next to the executable.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt6 6.5 QUIET COMPONENTS LinguistTools)

qt_standard_project_setup()

option(OGT_BUILD_TESTS "Build the core tests" ON)
option(OGT_BUILD_TOOLS "Build the snapshot tool" ON)
option(NODIRIS_PORTABLE_LINUX_PACKAGE "Bundle Qt under /opt/NodIris in the Debian package" OFF)

# The core knows nothing about widgets, so the tests run without a display.
add_library(ogt_core STATIC
    src/core/Graph.cpp
    src/core/NeuronKind.cpp
    src/core/ProjectFile.cpp
    src/core/Thumbnail.cpp
    src/core/Layout.cpp
)
target_include_directories(ogt_core PUBLIC src)
target_link_libraries(ogt_core PUBLIC Qt6::Core Qt6::Gui)

add_library(ogt_ui STATIC
    src/ui/Theme.cpp
    src/ui/AppearanceDialog.cpp
    src/ui/Icons.cpp
    src/ui/NeuronItem.cpp
    src/ui/LinkItem.cpp
    src/ui/GraphScene.cpp
    src/ui/GraphView.cpp
    src/ui/Commands.cpp
    src/ui/Inspector.cpp
    src/ui/DocumentPage.cpp
    src/ui/PreviewPopup.cpp
    src/ui/StartPage.cpp
    src/ui/MainWindow.cpp
    src/ui/WindowTitleBar.cpp
    src/ui/papicons.qrc
)
target_link_libraries(ogt_ui PUBLIC ogt_core Qt6::Widgets)

qt_add_executable(nodiris WIN32 MACOSX_BUNDLE src/main.cpp)
qt_add_resources(nodiris "app_assets"
    PREFIX "/"
    FILES packaging/icons/nodiris.png
)
if(WIN32)
    target_sources(nodiris PRIVATE packaging/windows/nodiris.rc)
endif()
target_link_libraries(nodiris PRIVATE ogt_ui)
target_compile_definitions(nodiris PRIVATE OGT_VERSION="${PROJECT_VERSION}")
set_target_properties(nodiris PROPERTIES
    OUTPUT_NAME NodIris
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.nodiris
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

if(Qt6LinguistTools_FOUND)
    qt_add_translations(nodiris
        TS_FILES translations/nodiris_fr.ts
        RESOURCE_PREFIX /i18n
    )
else()
    message(STATUS "Qt LinguistTools not found: the interface will only be in English")
endif()

if(MSVC)
    target_compile_options(ogt_core PRIVATE /W4)
    target_compile_options(ogt_ui PRIVATE /W4)
else()
    target_compile_options(ogt_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(ogt_ui PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(OGT_BUILD_TESTS)
    enable_testing()
    foreach(name graph_tests project_file_tests)
        add_executable(${name} tests/${name}.cpp)
        target_link_libraries(${name} PRIVATE ogt_core)
        add_test(NAME ${name} COMMAND ${name})
        set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endforeach()
    add_executable(scene_tests tests/scene_tests.cpp)
    target_link_libraries(scene_tests PRIVATE ogt_ui)
    add_test(NAME scene_tests COMMAND scene_tests)
endif()

if(OGT_BUILD_TOOLS)
    add_executable(nodiris_snapshot tools/snapshot.cpp)
    target_link_libraries(nodiris_snapshot PRIVATE ogt_ui)
endif()

install(TARGETS nodiris BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES Logo.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/nodiris
)
if(UNIX AND NOT APPLE)
    if(NODIRIS_PORTABLE_LINUX_PACKAGE)
        set(nodiris_desktop_executable "/opt/NodIris/bin/NodIris")
        set(nodiris_integration_prefix "/usr/share")
    else()
        set(nodiris_desktop_executable "NodIris")
        set(nodiris_integration_prefix "${CMAKE_INSTALL_DATADIR}")
    endif()
    configure_file(
        packaging/linux/io.github.honiway.NodIris.desktop.in
        ${CMAKE_CURRENT_BINARY_DIR}/io.github.honiway.NodIris.desktop
        @ONLY
    )
    install(FILES packaging/icons/nodiris.png
        DESTINATION ${nodiris_integration_prefix}/icons/hicolor/512x512/apps
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.honiway.NodIris.desktop
        DESTINATION ${nodiris_integration_prefix}/applications
    )
    install(FILES packaging/linux/io.github.honiway.NodIris.metainfo.xml
        DESTINATION ${nodiris_integration_prefix}/metainfo
    )
    install(FILES packaging/linux/io.github.honiway.NodIris.xml
        DESTINATION ${nodiris_integration_prefix}/mime/packages
    )
endif()
install(FILES LICENSE licenses/Papicons-LICENSE.txt
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
if(WIN32 OR APPLE OR NODIRIS_PORTABLE_LINUX_PACKAGE)
    qt_generate_deploy_app_script(
        TARGET nodiris
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()

set(CPACK_PACKAGE_NAME "NodIris")
set(CPACK_PACKAGE_VENDOR "Honiway")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A lightweight, local mind-mapping application")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Honiway/NodIris")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "NodIris")
set(CPACK_PACKAGE_EXECUTABLES "NodIris" "NodIris")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_FILE_NAME "NodIris-${PROJECT_VERSION}-win64-setup")
    set(CPACK_NSIS_DISPLAY_NAME "NodIris")
    set(CPACK_NSIS_PACKAGE_NAME "NodIris")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/nodiris.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/nodiris.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\NodIris.exe")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/Honiway/NodIris")
    set(CPACK_NSIS_CONTACT "Honiway")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_CREATE_DESKTOP_LINKS "NodIris")
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_FILE_NAME "nodiris-${PROJECT_VERSION}-linux-amd64")
    set(CPACK_DEBIAN_PACKAGE_NAME "nodiris")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Honiway")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/Honiway/NodIris")
    if(NODIRIS_PORTABLE_LINUX_PACKAGE)
        set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/NodIris")
    endif()
endif()

include(CPack)
