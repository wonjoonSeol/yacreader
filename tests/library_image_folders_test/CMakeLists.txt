qt_add_executable(library_image_folders_test
    main.cpp
)
yacreader_apply_build_options(library_image_folders_test)
target_link_libraries(library_image_folders_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
    Qt6::Test
    library_common
)

add_test(NAME library_image_folders_test COMMAND library_image_folders_test)
