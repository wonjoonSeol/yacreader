qt_add_executable(comics_deletion_test
    main.cpp
)
yacreader_apply_build_options(comics_deletion_test)
target_link_libraries(comics_deletion_test PRIVATE
    Qt6::Core
    Qt6::Test
    library_common
)

add_test(NAME comics_deletion_test COMMAND comics_deletion_test)
