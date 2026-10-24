# Resize image test

qt_add_executable(resize_image_test
    main.cpp
)
yacreader_apply_build_options(resize_image_test)
target_link_libraries(resize_image_test PRIVATE
    Qt6::Gui
    Qt6::Test
    image_processing
)
add_test(NAME resize_image_test COMMAND resize_image_test)
