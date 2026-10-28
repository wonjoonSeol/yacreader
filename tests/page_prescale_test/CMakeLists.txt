# Page prescale test

qt_add_executable(page_prescale_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/page_fit.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/page_fit.h
    ${PROJECT_SOURCE_DIR}/YACReader/page_prescaler.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/page_prescaler.h
    ${PROJECT_SOURCE_DIR}/YACReader/page_view.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/page_view.h
)
yacreader_apply_build_options(page_prescale_test)
target_include_directories(page_prescale_test PRIVATE
    ${PROJECT_SOURCE_DIR}/YACReader
)
target_link_libraries(page_prescale_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    image_processing
)
add_test(NAME page_prescale_test COMMAND page_prescale_test)
