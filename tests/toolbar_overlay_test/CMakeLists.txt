# Toolbar laid over the page test

qt_add_executable(toolbar_overlay_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/toolbar_over_page.h
    ${PROJECT_SOURCE_DIR}/YACReader/toolbar_overlay.h
    ${PROJECT_SOURCE_DIR}/YACReader/toolbar_overlay.cpp
)
yacreader_apply_build_options(toolbar_overlay_test)
target_include_directories(toolbar_overlay_test PRIVATE
    ${PROJECT_SOURCE_DIR}/YACReader
)
target_link_libraries(toolbar_overlay_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)
add_test(NAME toolbar_overlay_test COMMAND toolbar_overlay_test)
# the window test needs a platform, also on machines without a display
set_tests_properties(toolbar_overlay_test PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
