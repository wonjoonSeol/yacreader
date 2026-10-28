qt_add_executable(delete_comics_question_test
    main.cpp
    ${CMAKE_SOURCE_DIR}/YACReaderLibrary/delete_comics_question.cpp
)
yacreader_apply_build_options(delete_comics_question_test)
target_link_libraries(delete_comics_question_test PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
    library_common
)

add_test(NAME delete_comics_question_test COMMAND delete_comics_question_test)
set_tests_properties(delete_comics_question_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
