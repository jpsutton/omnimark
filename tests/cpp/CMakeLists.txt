find_package(Qt6 REQUIRED COMPONENTS Test)

function(omnimark_add_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE omnimark_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

omnimark_add_test(tst_localroots)
omnimark_add_test(tst_schemehandler)
omnimark_add_test(tst_filewatcher)
omnimark_add_test(tst_singleinstance)
omnimark_add_test(tst_search)
omnimark_add_test(tst_updater)

# GUI test with real QtWebEngine pages; needs the web frontend built.
qt_add_executable(tst_mainwindow tst_mainwindow.cpp)
target_link_libraries(tst_mainwindow PRIVATE omnimark_core Qt6::Test)
target_compile_definitions(tst_mainwindow PRIVATE OMNIMARK_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
if(TARGET omnimark_web)
    add_dependencies(tst_mainwindow omnimark_web)
endif()
add_test(NAME tst_mainwindow COMMAND tst_mainwindow)
set_tests_properties(tst_mainwindow PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu"
    TIMEOUT 180)
