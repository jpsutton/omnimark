cmake_minimum_required(VERSION 3.24)

project(omnimark VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(OMNIMARK_BUILD_WEB "Build the web frontend with pnpm as part of the build" ON)
option(OMNIMARK_BUILD_TESTS "Build C++ tests" ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent PrintSupport Svg WebEngineCore WebEngineWidgets WebChannel)
qt_standard_project_setup()

# ---------------------------------------------------------------------------
# Web frontend (Vite). Output lands in web/dist and is served via omnimark://app
# ---------------------------------------------------------------------------
set(OMNIMARK_WEB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/web")
set(OMNIMARK_WEB_DIST "${OMNIMARK_WEB_DIR}/dist")

if(OMNIMARK_BUILD_WEB)
    find_program(PNPM_EXECUTABLE pnpm REQUIRED)
    file(GLOB_RECURSE OMNIMARK_WEB_SOURCES CONFIGURE_DEPENDS "${OMNIMARK_WEB_DIR}/src/*")
    list(APPEND OMNIMARK_WEB_SOURCES "${OMNIMARK_WEB_DIR}/index.html")
    add_custom_command(
        OUTPUT "${OMNIMARK_WEB_DIR}/node_modules/.modules.yaml"
        COMMAND "${PNPM_EXECUTABLE}" install --frozen-lockfile
        WORKING_DIRECTORY "${OMNIMARK_WEB_DIR}"
        DEPENDS "${OMNIMARK_WEB_DIR}/package.json" "${OMNIMARK_WEB_DIR}/pnpm-lock.yaml"
        COMMENT "Installing web dependencies"
        VERBATIM)
    add_custom_command(
        OUTPUT "${OMNIMARK_WEB_DIST}/index.html"
        COMMAND "${PNPM_EXECUTABLE}" run build
        WORKING_DIRECTORY "${OMNIMARK_WEB_DIR}"
        DEPENDS ${OMNIMARK_WEB_SOURCES}
                "${OMNIMARK_WEB_DIR}/node_modules/.modules.yaml"
                "${OMNIMARK_WEB_DIR}/vite.config.ts"
        COMMENT "Building web frontend"
        VERBATIM)
    add_custom_target(omnimark_web ALL DEPENDS "${OMNIMARK_WEB_DIST}/index.html")
endif()

# ---------------------------------------------------------------------------
# Core library (shared by the app and the tests)
# ---------------------------------------------------------------------------
qt_add_library(omnimark_core STATIC
    src/Bridge.cpp src/Bridge.h
    src/DocumentTab.cpp src/DocumentTab.h
    src/Exporter.cpp src/Exporter.h
    src/FileWatcher.cpp src/FileWatcher.h
    src/FindBar.cpp src/FindBar.h
    src/Icons.cpp src/Icons.h
    src/LocalRoots.cpp src/LocalRoots.h
    src/MainWindow.cpp src/MainWindow.h
    src/SchemeHandler.cpp src/SchemeHandler.h
    src/SearchDock.cpp src/SearchDock.h
    src/Settings.cpp src/Settings.h
    src/SettingsDialog.cpp src/SettingsDialog.h
    src/SingleInstance.cpp src/SingleInstance.h
    src/TocDock.cpp src/TocDock.h
    src/WebPage.cpp src/WebPage.h
    src/WebAssets.cpp src/WebAssets.h
    src/WorkspaceDock.cpp src/WorkspaceDock.h
)
target_include_directories(omnimark_core PUBLIC src)
target_compile_definitions(omnimark_core
    PRIVATE OMNIMARK_SOURCE_WEB_DIST="${OMNIMARK_WEB_DIST}"
            OMNIMARK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    PUBLIC OMNIMARK_VERSION="${PROJECT_VERSION}")
target_link_libraries(omnimark_core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent Qt6::PrintSupport Qt6::WebEngineCore Qt6::WebEngineWidgets Qt6::WebChannel)

set(OMNIMARK_APP_ID "io.github.omnimark.Omnimark")
set(OMNIMARK_ICON_SIZES 16 32 48 64 128 256 512)

set(OMNIMARK_PLATFORM_SOURCES)
if(WIN32)
    set(OMNIMARK_ICO "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/omnimark.ico")
    configure_file(packaging/windows/omnimark.rc.in "${CMAKE_CURRENT_BINARY_DIR}/omnimark.rc" @ONLY)
    list(APPEND OMNIMARK_PLATFORM_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/omnimark.rc")
elseif(APPLE)
    set(OMNIMARK_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/omnimark.icns")
    set_source_files_properties("${OMNIMARK_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND OMNIMARK_PLATFORM_SOURCES "${OMNIMARK_ICNS}")
endif()

qt_add_executable(omnimark src/main.cpp ${OMNIMARK_PLATFORM_SOURCES})
target_link_libraries(omnimark PRIVATE omnimark_core)
target_compile_definitions(omnimark PRIVATE OMNIMARK_APP_ID="${OMNIMARK_APP_ID}")
if(OMNIMARK_BUILD_WEB)
    add_dependencies(omnimark omnimark_web)
endif()

# Window icon, embedded so it works before (or without) installation.
set(OMNIMARK_ICON_FILES)
foreach(size IN LISTS OMNIMARK_ICON_SIZES)
    list(APPEND OMNIMARK_ICON_FILES "packaging/icons/omnimark-${size}.png")
endforeach()
qt_add_resources(omnimark "icons" PREFIX "/" FILES ${OMNIMARK_ICON_FILES})
# Toolbar icons used when the platform/theme has no match (Windows, macOS, AppImage).
file(GLOB OMNIMARK_UI_ICONS CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" resources/icons/*.svg)
qt_add_resources(omnimark "ui_icons" PREFIX "/" FILES ${OMNIMARK_UI_ICONS})
# Linked so deployment tools ship the SVG icon engine.
target_link_libraries(omnimark PRIVATE Qt6::Svg)

configure_file(packaging/macos/Info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)
set_target_properties(omnimark PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    OUTPUT_NAME omnimark)

if(APPLE)
    # The frontend lives in Contents/Resources/web (see WebAssets::directory).
    add_custom_command(TARGET omnimark POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${OMNIMARK_WEB_DIST}"
                "$<TARGET_BUNDLE_CONTENT_DIR:omnimark>/Resources/web"
        COMMENT "Copying web frontend into the app bundle")
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

set(OMNIMARK_DOC_FILES LICENSE THIRD_PARTY_NOTICES.md)
if(APPLE)
    install(TARGETS omnimark BUNDLE DESTINATION .)
    install(FILES ${OMNIMARK_DOC_FILES} DESTINATION omnimark.app/Contents/Resources)
elseif(WIN32)
    install(TARGETS omnimark RUNTIME DESTINATION .)
    install(DIRECTORY "${OMNIMARK_WEB_DIST}/" DESTINATION web)
    install(FILES ${OMNIMARK_DOC_FILES} DESTINATION .)
else()
    install(FILES ${OMNIMARK_DOC_FILES} DESTINATION "${CMAKE_INSTALL_DOCDIR}")
    install(TARGETS omnimark RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY "${OMNIMARK_WEB_DIST}/" DESTINATION "${CMAKE_INSTALL_DATADIR}/omnimark/web")
    install(FILES packaging/linux/${OMNIMARK_APP_ID}.desktop
            DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES packaging/linux/${OMNIMARK_APP_ID}.metainfo.xml
            DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
    install(FILES packaging/linux/omnimark-mime.xml
            DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages")
    install(FILES packaging/icons/omnimark.svg
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
            RENAME "${OMNIMARK_APP_ID}.svg")
    foreach(size IN LISTS OMNIMARK_ICON_SIZES)
        install(FILES packaging/icons/omnimark-${size}.png
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps"
                RENAME "${OMNIMARK_APP_ID}.png")
    endforeach()
endif()

# Bundle Qt next to the app on Windows/macOS (windeployqt / macdeployqt).
if(WIN32 OR APPLE)
    qt_generate_deploy_app_script(
        TARGET omnimark
        OUTPUT_SCRIPT OMNIMARK_DEPLOY_SCRIPT
        NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT "${OMNIMARK_DEPLOY_SCRIPT}")
endif()

# ---------------------------------------------------------------------------
# Packages (cpack)
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "Omnimark")
set(CPACK_PACKAGE_VENDOR "Omnimark")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Markdown viewer and editor")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Omnimark")
set(CPACK_PACKAGE_EXECUTABLES "omnimark" "Omnimark")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_MUI_ICON "${OMNIMARK_ICO}")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "omnimark.exe")
    set(CPACK_NSIS_DISPLAY_NAME "Omnimark")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\$STARTMENU_FOLDER\\Omnimark.lnk' '$INSTDIR\\omnimark.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '$SMPROGRAMS\\$START_MENU\\Omnimark.lnk'")
    # Register "Open with Omnimark" for Markdown files.
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS [=[
  WriteRegStr HKCU "Software\Classes\Omnimark.Markdown" "" "Markdown Document"
  WriteRegStr HKCU "Software\Classes\Omnimark.Markdown\DefaultIcon" "" "$INSTDIR\omnimark.exe,0"
  WriteRegStr HKCU "Software\Classes\Omnimark.Markdown\shell\open\command" "" '"$INSTDIR\omnimark.exe" "%1"'
  WriteRegStr HKCU "Software\Classes\.md\OpenWithProgids" "Omnimark.Markdown" ""
  WriteRegStr HKCU "Software\Classes\.markdown\OpenWithProgids" "Omnimark.Markdown" ""
  WriteRegStr HKCU "Software\Classes\Applications\omnimark.exe\SupportedTypes" ".md" ""
  WriteRegStr HKCU "Software\Classes\Applications\omnimark.exe\SupportedTypes" ".markdown" ""
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
]=])
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS [=[
  DeleteRegKey HKCU "Software\Classes\Omnimark.Markdown"
  DeleteRegValue HKCU "Software\Classes\.md\OpenWithProgids" "Omnimark.Markdown"
  DeleteRegValue HKCU "Software\Classes\.markdown\OpenWithProgids" "Omnimark.Markdown"
  DeleteRegKey HKCU "Software\Classes\Applications\omnimark.exe"
]=])
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Omnimark")
else()
    set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)

if(OMNIMARK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/cpp)
endif()
