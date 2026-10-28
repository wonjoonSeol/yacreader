# ACNet upscale test

qt_add_executable(acnet_upscale_test
    main.cpp
)
yacreader_apply_build_options(acnet_upscale_test)
target_link_libraries(acnet_upscale_test PRIVATE
    Qt6::Gui
    Qt6::Test
    image_processing
    anime4kcpp
)
add_test(NAME acnet_upscale_test COMMAND acnet_upscale_test)
