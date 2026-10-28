# Anime4KCPP thread pool test, in its own process: the pool is created once, by the first upscale

qt_add_executable(acnet_thread_pool_test
    main.cpp
)
yacreader_apply_build_options(acnet_thread_pool_test)
target_link_libraries(acnet_thread_pool_test PRIVATE
    Qt6::Gui
    Qt6::Test
    image_processing
    anime4kcpp
)
add_test(NAME acnet_thread_pool_test COMMAND acnet_thread_pool_test)
