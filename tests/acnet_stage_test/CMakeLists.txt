# ACNet prescale stage test

qt_add_executable(acnet_stage_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/acnet_stage.h
    ${PROJECT_SOURCE_DIR}/YACReader/page_prescaler.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/page_prescaler.h
    ${PROJECT_SOURCE_DIR}/YACReader/page_view.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/page_view.h
)
yacreader_apply_build_options(acnet_stage_test)
target_include_directories(acnet_stage_test PRIVATE
    ${PROJECT_SOURCE_DIR}/YACReader
)
target_link_libraries(acnet_stage_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    image_processing
)
add_test(NAME acnet_stage_test COMMAND acnet_stage_test)
