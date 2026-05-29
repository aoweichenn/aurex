add_library(aurex_project
    src/project/project_model.cpp
)
target_link_libraries(aurex_project PUBLIC aurex_base aurex_query)
target_include_directories(aurex_project PUBLIC include)
