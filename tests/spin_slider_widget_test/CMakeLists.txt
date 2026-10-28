# Spin slider widget test

qt_add_executable(spin_slider_widget_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/custom_widgets/yacreader_spin_slider_widget.cpp
    ${PROJECT_SOURCE_DIR}/custom_widgets/yacreader_spin_slider_widget.h
)
yacreader_apply_build_options(spin_slider_widget_test)
target_include_directories(spin_slider_widget_test PRIVATE
    ${PROJECT_SOURCE_DIR}/custom_widgets
)
target_link_libraries(spin_slider_widget_test PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
)
add_test(NAME spin_slider_widget_test COMMAND spin_slider_widget_test)
# widgets need a platform, also on machines without a display
set_tests_properties(spin_slider_widget_test PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
