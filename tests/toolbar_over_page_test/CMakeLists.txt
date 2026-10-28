# Toolbar over the page in full screen test

qt_add_executable(toolbar_over_page_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/toolbar_over_page.h
)
yacreader_apply_build_options(toolbar_over_page_test)
target_include_directories(toolbar_over_page_test PRIVATE
    ${PROJECT_SOURCE_DIR}/YACReader
)
target_link_libraries(toolbar_over_page_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)
add_test(NAME toolbar_over_page_test COMMAND toolbar_over_page_test)
