# Scaling method option test: the choices, the Options combo and the stored setting

qt_add_executable(image_adjustment_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/configuration.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/configuration.h
    ${PROJECT_SOURCE_DIR}/YACReader/image_adjustment.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/image_adjustment.h
)
yacreader_apply_build_options(image_adjustment_test)
target_include_directories(image_adjustment_test PRIVATE
    ${PROJECT_SOURCE_DIR}/YACReader
)
target_link_libraries(image_adjustment_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    image_processing
    yr_global
)
add_test(NAME image_adjustment_test COMMAND image_adjustment_test)
set_tests_properties(image_adjustment_test PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
