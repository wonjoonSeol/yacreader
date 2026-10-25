# Image adjustment choices test

qt_add_executable(image_adjustment_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/image_adjustment.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/image_adjustment.h
)
yacreader_apply_build_options(image_adjustment_test)
target_include_directories(image_adjustment_test PRIVATE
    ${PROJECT_SOURCE_DIR}/YACReader
    ${PROJECT_SOURCE_DIR}/image_processing
)
target_link_libraries(image_adjustment_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    image_processing
)
add_test(NAME image_adjustment_test COMMAND image_adjustment_test)
