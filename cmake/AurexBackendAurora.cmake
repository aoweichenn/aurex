add_library(aurex_backend_aurora
    src/backend/aurora/aurora_backend.cpp
    src/backend/aurora/aurora_type_translator.cpp
    src/backend/aurora/aurora_ir_translator.cpp
    src/driver/aurora_codegen_backend.cpp
)
target_link_libraries(aurex_backend_aurora PUBLIC
    aurex_base
    aurex_ir
    AuroraAir
    AuroraX86
    AuroraCodeGen
    AuroraMC
)
target_include_directories(aurex_backend_aurora PUBLIC include)
