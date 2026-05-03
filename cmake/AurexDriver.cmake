add_library(m0_driver
    src/driver/compiler.cpp
    src/driver/module_loader.cpp
)
target_link_libraries(m0_driver PUBLIC m0_base m0_syntax m0_lex m0_parse m0_sema m0_codegen_c)
target_include_directories(m0_driver PUBLIC include)
