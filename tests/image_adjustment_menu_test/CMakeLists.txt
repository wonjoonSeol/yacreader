# Image adjustment menu and Options test

qt_add_executable(image_adjustment_menu_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/configuration.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/configuration.h
    ${PROJECT_SOURCE_DIR}/YACReader/image_adjustment.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/image_adjustment.h
    ${PROJECT_SOURCE_DIR}/YACReader/image_adjustment_menu.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/image_adjustment_menu.h
    ${PROJECT_SOURCE_DIR}/YACReader/image_filter_slider.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/image_filter_slider.h
    ${PROJECT_SOURCE_DIR}/YACReader/options_dialog.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/options_dialog.h
    ${PROJECT_SOURCE_DIR}/YACReader/themes/theme_factory.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/themes/theme_factory.h
    ${PROJECT_SOURCE_DIR}/common/themes/theme_manager.cpp
    ${PROJECT_SOURCE_DIR}/common/themes/theme_manager.h
)
yacreader_apply_build_options(image_adjustment_menu_test)
target_include_directories(image_adjustment_menu_test PRIVATE
    ${PROJECT_SOURCE_DIR}/YACReader
    ${PROJECT_SOURCE_DIR}/YACReader/themes
)
target_compile_definitions(image_adjustment_menu_test PRIVATE YACREADER)
target_link_libraries(image_adjustment_menu_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    common_gui
    custom_widgets_reader
    image_processing
)
add_test(NAME image_adjustment_menu_test COMMAND image_adjustment_menu_test)
# the menu and Options need a platform, also on machines without a display
set_tests_properties(image_adjustment_menu_test PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
