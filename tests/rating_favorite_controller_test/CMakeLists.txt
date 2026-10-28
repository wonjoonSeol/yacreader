qt_add_executable(rating_favorite_controller_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/YACReader/rating_favorite_controller.h
    ${PROJECT_SOURCE_DIR}/YACReader/rating_favorite_controller.cpp
)
yacreader_apply_build_options(rating_favorite_controller_test)
target_include_directories(rating_favorite_controller_test PRIVATE
    ${PROJECT_SOURCE_DIR}/YACReader
)
target_link_libraries(rating_favorite_controller_test PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
    Qt6::Widgets
    common_all
    common_gui
    shortcuts_reader
)

add_test(NAME rating_favorite_controller_test COMMAND rating_favorite_controller_test)
