qt_add_executable(shortcut_defaults_test
    main.cpp
)
yacreader_apply_build_options(shortcut_defaults_test)
target_link_libraries(shortcut_defaults_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    shortcuts_reader
)

add_test(NAME shortcut_defaults_test COMMAND shortcut_defaults_test)
