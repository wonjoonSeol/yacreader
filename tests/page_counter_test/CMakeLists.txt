# Page counter test

qt_add_executable(page_counter_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/pages_information.h
    ${PROJECT_SOURCE_DIR}/YACReader/page_counter_trigger.h
)
yacreader_apply_build_options(page_counter_test)
target_include_directories(page_counter_test PRIVATE
    ${PROJECT_SOURCE_DIR}/YACReader
)
target_link_libraries(page_counter_test PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME page_counter_test COMMAND page_counter_test)
