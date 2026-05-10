#include <aurex/ir/enum_layout.hpp>

namespace aurex::ir {

bool is_payload_enum(const sema::TypeTable& types, const sema::TypeHandle type) noexcept {
    return sema::is_valid(type) &&
           types.get(type).kind == sema::TypeKind::enum_ &&
           sema::is_valid(types.get(type).enum_payload_storage);
}

sema::TypeHandle enum_tag_type(const sema::TypeTable& types, const sema::TypeHandle enum_type) noexcept {
    if (!sema::is_valid(enum_type) || types.get(enum_type).kind != sema::TypeKind::enum_) {
        return sema::INVALID_TYPE_HANDLE;
    }
    return types.get(enum_type).enum_underlying;
}

sema::TypeHandle enum_payload_storage_type(const sema::TypeTable& types, const sema::TypeHandle enum_type) noexcept {
    if (!is_payload_enum(types, enum_type)) {
        return sema::INVALID_TYPE_HANDLE;
    }
    return types.get(enum_type).enum_payload_storage;
}

RecordLayout make_payload_enum_record(const sema::TypeTable& types, const sema::TypeHandle enum_type) {
    RecordLayout record;
    record.type = enum_type;
    record.name = types.display_name(enum_type);
    record.symbol = types.c_name(enum_type);
    record.fields.push_back(RecordField {
        std::string(IR_ENUM_TAG_FIELD_NAME),
        enum_tag_type(types, enum_type),
    });
    record.fields.push_back(RecordField {
        std::string(IR_ENUM_PAYLOAD_FIELD_NAME),
        enum_payload_storage_type(types, enum_type),
    });
    return record;
}

} // namespace aurex::ir
