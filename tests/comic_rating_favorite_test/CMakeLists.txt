qt_add_executable(comic_rating_favorite_test
    main.cpp
)
yacreader_apply_build_options(comic_rating_favorite_test)
target_link_libraries(comic_rating_favorite_test PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
    db_helper
)

add_test(NAME comic_rating_favorite_test COMMAND comic_rating_favorite_test)
