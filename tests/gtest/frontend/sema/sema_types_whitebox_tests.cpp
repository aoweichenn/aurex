#include <gtest/frontend/sema/sema_whitebox_test_support.hpp>

namespace aurex::test {
namespace {

class CanonicalTypeBuilderTestResolver final : public sema::CanonicalTypeKeyResolver {
public:
    void bind_nominal(const TypeHandle handle, const query::DefKey key)
    {
        this->nominal_keys_.emplace(handle.value, key);
    }

    void bind_generic(const sema::GenericParamIdentity identity, const query::GenericParamKey key)
    {
        this->generic_param_keys_.emplace(identity.value, key);
    }

    [[nodiscard]] std::optional<query::DefKey> nominal_type_key(
        const TypeHandle handle, const sema::TypeInfo& info) const override
    {
        static_cast<void>(info);
        const auto found = this->nominal_keys_.find(handle.value);
        if (found == this->nominal_keys_.end()) {
            return std::nullopt;
        }
        return found->second;
    }

    [[nodiscard]] std::optional<query::GenericParamKey> generic_param_key(
        const TypeHandle handle, const sema::TypeInfo& info) const override
    {
        static_cast<void>(handle);
        const auto found = this->generic_param_keys_.find(info.generic_identity.value);
        if (found == this->generic_param_keys_.end()) {
            return std::nullopt;
        }
        return found->second;
    }

private:
    std::unordered_map<base::u32, query::DefKey> nominal_keys_;
    std::unordered_map<base::u64, query::GenericParamKey> generic_param_keys_;
};

} // namespace

TEST(CoreUnit, CanonicalTypeBuilderMapsBuiltinTypesWithoutSessionHandles)
{
    sema::TypeTable types;
    CanonicalTypeBuilderTestResolver resolver;

    const std::array<std::pair<BuiltinType, query::BuiltinTypeKey>, 16> builtins{{
        {BuiltinType::void_, query::BuiltinTypeKey::void_},
        {BuiltinType::bool_, query::BuiltinTypeKey::bool_},
        {BuiltinType::i8, query::BuiltinTypeKey::i8},
        {BuiltinType::u8, query::BuiltinTypeKey::u8},
        {BuiltinType::i16, query::BuiltinTypeKey::i16},
        {BuiltinType::u16, query::BuiltinTypeKey::u16},
        {BuiltinType::i32, query::BuiltinTypeKey::i32},
        {BuiltinType::u32, query::BuiltinTypeKey::u32},
        {BuiltinType::i64, query::BuiltinTypeKey::i64},
        {BuiltinType::u64, query::BuiltinTypeKey::u64},
        {BuiltinType::isize, query::BuiltinTypeKey::isize},
        {BuiltinType::usize, query::BuiltinTypeKey::usize},
        {BuiltinType::f32, query::BuiltinTypeKey::f32},
        {BuiltinType::f64, query::BuiltinTypeKey::f64},
        {BuiltinType::str, query::BuiltinTypeKey::str},
        {BuiltinType::char_, query::BuiltinTypeKey::char_},
    }};

    for (const auto& [source, expected] : builtins) {
        base::Result<query::CanonicalTypeKey> key =
            sema::build_canonical_type_key(types, types.builtin(source), resolver);
        ASSERT_TRUE(key.has_value());
        EXPECT_EQ(key.value(), query::canonical_builtin(expected));
    }
}
TEST(CoreUnit, CanonicalTypeBuilderMapsCompoundNominalAndGenericTypes)
{
    sema::TypeTable types;
    CanonicalTypeBuilderTestResolver resolver;
    const query::ModuleKey module = query_test_module_key();
    const query::DefKey vector_def = query_test_def_key(module, query::DefKind::struct_, "Vec");
    const query::DefKey enum_def = query_test_def_key(module, query::DefKind::enum_, "Choice");
    const query::DefKey opaque_def = query_test_def_key(module, query::DefKind::struct_, "Opaque");
    const query::GenericParamKey type_param_key = query::generic_param_key(vector_def, 0);
    const sema::GenericParamIdentity type_param_identity = sema::generic_param_identity_from_text("types.Vec.T");

    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle type_param = types.generic_param(type_param_identity, "T");
    const TypeHandle vector_type = types.named_struct("Vec", "Vec", false);
    const TypeHandle enum_type = types.named_enum("Choice", "Choice");
    const TypeHandle opaque_type = types.opaque_struct("Opaque", "Opaque");
    const std::array<TypeHandle, 1> vector_args{type_param};
    types.set_generic_instance(vector_type, "Vec", vector_args);
    resolver.bind_nominal(vector_type, vector_def);
    resolver.bind_nominal(enum_type, enum_def);
    resolver.bind_nominal(opaque_type, opaque_def);
    resolver.bind_generic(type_param_identity, type_param_key);

    const TypeHandle array_bool = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, bool_type);
    const TypeHandle pointer_vector = types.pointer(PointerMutability::mut, vector_type);
    const TypeHandle slice_i32 = types.slice(PointerMutability::mut, i32);
    const TypeHandle reference_slice = types.reference(PointerMutability::const_, slice_i32);
    const std::array<TypeHandle, 3> tuple_elements{array_bool, pointer_vector, reference_slice};
    const TypeHandle tuple_type = types.tuple(tuple_elements);
    const std::array<TypeHandle, 2> function_params{tuple_type, enum_type};
    const TypeHandle function_type =
        types.function(sema::FunctionCallConv::c, true, true, function_params, opaque_type);
    const TypeHandle empty_function_type =
        types.function(sema::FunctionCallConv::aurex, false, false, std::span<const TypeHandle>{}, i32);

    base::Result<query::CanonicalTypeKey> key = sema::build_canonical_type_key(types, function_type, resolver);
    ASSERT_TRUE(key.has_value());
    base::Result<query::CanonicalTypeKey> empty_function_key =
        sema::build_canonical_type_key(types, empty_function_type, resolver);
    ASSERT_TRUE(empty_function_key.has_value());

    const query::CanonicalTypeKey expected_type_param = query::canonical_generic_param(type_param_key);
    const std::array<query::CanonicalTypeKey, 1> expected_vector_args{expected_type_param};
    const query::CanonicalTypeKey expected_vector = query::canonical_nominal(vector_def, expected_vector_args);
    const query::CanonicalTypeKey expected_array_bool =
        query::canonical_array(SEMA_TEST_SMALL_ARRAY_COUNT, query::canonical_builtin(query::BuiltinTypeKey::bool_));
    const query::CanonicalTypeKey expected_pointer_vector =
        query::canonical_pointer(query::PointerMutabilityKey::mut, expected_vector);
    const query::CanonicalTypeKey expected_slice_i32 =
        query::canonical_slice(query::PointerMutabilityKey::mut, query::canonical_builtin(query::BuiltinTypeKey::i32));
    const query::CanonicalTypeKey expected_reference_slice =
        query::canonical_reference(query::PointerMutabilityKey::const_, expected_slice_i32);
    const std::array<query::CanonicalTypeKey, 3> expected_tuple_elements{
        expected_array_bool,
        expected_pointer_vector,
        expected_reference_slice,
    };
    const query::CanonicalTypeKey expected_tuple = query::canonical_tuple(expected_tuple_elements);
    const query::CanonicalTypeKey expected_enum =
        query::canonical_nominal(enum_def, std::span<const query::CanonicalTypeKey>{});
    const query::CanonicalTypeKey expected_opaque =
        query::canonical_nominal(opaque_def, std::span<const query::CanonicalTypeKey>{});
    const std::array<query::CanonicalTypeKey, 2> expected_params{expected_tuple, expected_enum};
    const query::CanonicalTypeKey expected_function =
        query::canonical_function(query::FunctionCallConvKey::c, true, true, expected_params, expected_opaque);
    const query::CanonicalTypeKey expected_empty_function = query::canonical_function(query::FunctionCallConvKey::aurex,
        false, false, std::span<const query::CanonicalTypeKey>{}, query::canonical_builtin(query::BuiltinTypeKey::i32));

    EXPECT_EQ(key.value(), expected_function);
    EXPECT_EQ(empty_function_key.value(), expected_empty_function);
    EXPECT_EQ(query::stable_key_fingerprint(key.value()), query::stable_key_fingerprint(expected_function));
}
TEST(CoreUnit, CanonicalTypeBuilderMapsAssociatedProjectionTypes)
{
    sema::TypeTable types;
    CanonicalTypeBuilderTestResolver resolver;
    const query::ModuleKey module = query_test_module_key();
    const std::array<std::string_view, 1> source_path{"Source"};
    const query::DefKey source_def =
        query::def_key(module, query::DefNamespace::trait_, query::DefKind::trait_, source_path);
    const query::MemberKey item_member =
        query::member_key(source_def, query::MemberKind::associated_type, "Item", SEMA_TEST_PATTERN_FIRST_INDEX);
    const sema::GenericParamIdentity type_param_identity = sema::generic_param_identity_from_text("types.Source.T");
    const query::GenericParamKey type_param_key = query::generic_param_key(source_def, SEMA_TEST_PATTERN_FIRST_INDEX);

    const TypeHandle type_param = types.generic_param(type_param_identity, "T");
    const TypeHandle projection = types.associated_projection(type_param, item_member, "Item");
    resolver.bind_generic(type_param_identity, type_param_key);

    base::Result<query::CanonicalTypeKey> key = sema::build_canonical_type_key(types, projection, resolver);
    ASSERT_TRUE(key.has_value());
    const query::CanonicalTypeKey expected =
        query::canonical_associated_type_projection(query::canonical_generic_param(type_param_key), item_member);
    EXPECT_EQ(key.value(), expected);

    TypeTableTestAccess::entries(types)[projection.value].associated_member = query::MemberKey{};
    base::Result<query::CanonicalTypeKey> invalid_member = sema::build_canonical_type_key(types, projection, resolver);
    EXPECT_FALSE(invalid_member.has_value());
    EXPECT_NE(invalid_member.error().message.find("unresolved associated type member"), std::string::npos);
}
TEST(CoreUnit, CanonicalTypeBuilderRejectsUnresolvedOrInvalidTypes)
{
    sema::TypeTable types;
    CanonicalTypeBuilderTestResolver resolver;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle unresolved_struct = types.named_struct("Missing", "Missing", false);
    const sema::GenericParamIdentity type_param_identity = sema::generic_param_identity_from_text("types.Missing.T");
    const TypeHandle unresolved_generic = types.generic_param(type_param_identity, "T");
    const TypeHandle array_invalid = types.array(SEMA_TEST_INVALID_ARRAY_COUNT, INVALID_TYPE_HANDLE);
    const TypeHandle array_unknown = types.array(SEMA_TEST_INVALID_ARRAY_COUNT,
        TypeHandle{static_cast<base::u32>(types.size() + SEMA_TEST_MISSING_MODULE_INDEX)});
    const TypeHandle pointer_i32 = types.pointer(PointerMutability::mut, i32);
    const TypeHandle reference_i32 = types.reference(PointerMutability::const_, i32);
    const TypeHandle slice_i32 = types.slice(PointerMutability::mut, i32);
    const TypeHandle function_i32 =
        types.function(sema::FunctionCallConv::aurex, false, false, std::span<const TypeHandle>{}, i32);

    base::Result<query::CanonicalTypeKey> invalid =
        sema::build_canonical_type_key(types, INVALID_TYPE_HANDLE, resolver);
    EXPECT_FALSE(invalid.has_value());
    EXPECT_NE(invalid.error().message.find("invalid type handle"), std::string::npos);

    base::Result<query::CanonicalTypeKey> unknown = sema::build_canonical_type_key(
        types, TypeHandle{static_cast<base::u32>(types.size() + SEMA_TEST_MISSING_MODULE_INDEX)}, resolver);
    EXPECT_FALSE(unknown.has_value());
    EXPECT_NE(unknown.error().message.find("unknown type handle"), std::string::npos);

    base::Result<query::CanonicalTypeKey> missing_nominal =
        sema::build_canonical_type_key(types, unresolved_struct, resolver);
    EXPECT_FALSE(missing_nominal.has_value());
    EXPECT_NE(missing_nominal.error().message.find("unresolved nominal type key"), std::string::npos);

    base::Result<query::CanonicalTypeKey> missing_generic =
        sema::build_canonical_type_key(types, unresolved_generic, resolver);
    EXPECT_FALSE(missing_generic.has_value());
    EXPECT_NE(missing_generic.error().message.find("unresolved generic parameter key"), std::string::npos);

    base::Result<query::CanonicalTypeKey> invalid_child =
        sema::build_canonical_type_key(types, array_invalid, resolver);
    EXPECT_FALSE(invalid_child.has_value());
    EXPECT_NE(invalid_child.error().message.find("invalid type handle"), std::string::npos);

    base::Result<query::CanonicalTypeKey> unknown_child =
        sema::build_canonical_type_key(types, array_unknown, resolver);
    EXPECT_FALSE(unknown_child.has_value());
    EXPECT_NE(unknown_child.error().message.find("unknown type handle"), std::string::npos);

    TypeTableTestAccess::entries(types)[i32.value].builtin = SEMA_TEST_INVALID_BUILTIN_TYPE;
    base::Result<query::CanonicalTypeKey> unsupported_builtin = sema::build_canonical_type_key(types, i32, resolver);
    EXPECT_FALSE(unsupported_builtin.has_value());
    EXPECT_NE(unsupported_builtin.error().message.find("unsupported builtin type"), std::string::npos);
    TypeTableTestAccess::entries(types)[i32.value].builtin = BuiltinType::i32;

    TypeTableTestAccess::entries(types)[pointer_i32.value].pointer_mutability = SEMA_TEST_INVALID_POINTER_MUTABILITY;
    base::Result<query::CanonicalTypeKey> unsupported_pointer_mutability =
        sema::build_canonical_type_key(types, pointer_i32, resolver);
    EXPECT_FALSE(unsupported_pointer_mutability.has_value());
    EXPECT_NE(unsupported_pointer_mutability.error().message.find("unsupported pointer mutability"), std::string::npos);

    TypeTableTestAccess::entries(types)[reference_i32.value].pointer_mutability = SEMA_TEST_INVALID_POINTER_MUTABILITY;
    base::Result<query::CanonicalTypeKey> unsupported_reference_mutability =
        sema::build_canonical_type_key(types, reference_i32, resolver);
    EXPECT_FALSE(unsupported_reference_mutability.has_value());
    EXPECT_NE(
        unsupported_reference_mutability.error().message.find("unsupported pointer mutability"), std::string::npos);

    TypeTableTestAccess::entries(types)[slice_i32.value].slice_mutability = SEMA_TEST_INVALID_POINTER_MUTABILITY;
    base::Result<query::CanonicalTypeKey> unsupported_slice_mutability =
        sema::build_canonical_type_key(types, slice_i32, resolver);
    EXPECT_FALSE(unsupported_slice_mutability.has_value());
    EXPECT_NE(unsupported_slice_mutability.error().message.find("unsupported pointer mutability"), std::string::npos);

    TypeTableTestAccess::entries(types)[function_i32.value].function_call_conv = SEMA_TEST_INVALID_FUNCTION_CALL_CONV;
    base::Result<query::CanonicalTypeKey> unsupported_call_conv =
        sema::build_canonical_type_key(types, function_i32, resolver);
    EXPECT_FALSE(unsupported_call_conv.has_value());
    EXPECT_NE(unsupported_call_conv.error().message.find("unsupported function call convention"), std::string::npos);

    TypeTableTestAccess::entries(types)[i32.value].kind = SEMA_TEST_INVALID_TYPE_KIND;
    base::Result<query::CanonicalTypeKey> unsupported = sema::build_canonical_type_key(types, i32, resolver);
    EXPECT_FALSE(unsupported.has_value());
    EXPECT_NE(unsupported.error().message.find("unsupported type kind"), std::string::npos);
}
TEST(CoreUnit, SemanticWhiteBoxLayoutPlacesAndModules)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
        module_info({"lib", "two"}),
        module_info({"lib", "reexported"}),
    };
    module.modules[0].imports = {
        resolved_import(module_id(1), "one"),
        resolved_import(module_id(2), "two"),
        resolved_import(syntax::INVALID_MODULE_ID, "broken"),
    };
    module.modules[1].imports = {
        resolved_import(module_id(3), "pub", syntax::Visibility::public_),
    };

    const ExprId value_expr = push_name(module, "value");
    const ExprId scoped_value_expr = push_name(module, "shared", "one");
    const ExprId missing_scoped_value_expr = push_name(module, "shared", "missing");
    const ExprId ptr_expr = push_name(module, "ptr");
    const ExprId record_expr = push_name(module, "record");
    const ExprId array_expr = push_name(module, "array");
    const ExprId field_id = push_field(module, ptr_expr, "field");
    const ExprId value_field_id = push_field(module, record_expr, "field");
    const ExprId nested_value_field_id = push_field(module, value_field_id, "field");
    const ExprId index_arg = push_integer(module);
    const ExprId index_id = module.push_index_expr({}, syntax::IndexExprPayload{ptr_expr, index_arg});
    const ExprId value_index_id = module.push_index_expr({}, syntax::IndexExprPayload{array_expr, index_arg});
    const ExprId nested_value_index_id =
        module.push_index_expr({}, syntax::IndexExprPayload{value_index_id, index_arg});
    const ExprId deref_id = push_unary(module, syntax::UnaryOp::dereference, ptr_expr);
    const ExprId invalid_deref_id = push_unary(module, syntax::UnaryOp::dereference, value_expr);
    const ExprId not_id = push_unary(module, syntax::UnaryOp::logical_not, ptr_expr);
    const ExprId array_ref_expr = push_name(module, "array_ref");
    const ExprId array_ref_index_id = module.push_index_expr({}, syntax::IndexExprPayload{array_ref_expr, index_arg});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(0);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle i8 = types.builtin(BuiltinType::i8);
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle i16 = types.builtin(BuiltinType::i16);
    const TypeHandle u16 = types.builtin(BuiltinType::u16);
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle u32 = types.builtin(BuiltinType::u32);
    const TypeHandle i64 = types.builtin(BuiltinType::i64);
    const TypeHandle u64 = types.builtin(BuiltinType::u64);
    const TypeHandle isize = types.builtin(BuiltinType::isize);
    const TypeHandle usize = types.builtin(BuiltinType::usize);
    const TypeHandle f32 = types.builtin(BuiltinType::f32);
    const TypeHandle f64 = types.builtin(BuiltinType::f64);
    const TypeHandle str = types.builtin(BuiltinType::str);
    const TypeHandle char_type = types.builtin(BuiltinType::char_);
    const TypeHandle ptr_i32 = types.pointer(PointerMutability::mut, i32);
    const TypeHandle const_ptr_i32 = types.pointer(PointerMutability::const_, i32);
    const TypeHandle ref_i32 = types.reference(PointerMutability::mut, i32);
    const TypeHandle ref_u32 = types.reference(PointerMutability::mut, u32);
    const TypeHandle slice_i32 = types.slice(PointerMutability::mut, i32);
    const TypeHandle slice_u32 = types.slice(PointerMutability::mut, u32);
    const TypeHandle array_i16 = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, i16);
    const TypeHandle missing_struct = types.named_struct("missing.Struct", "missing_Struct", false);
    const TypeHandle record_type = types.named_struct("lib.one.Record", "lib_one_Record", false);
    const TypeHandle enum_type = types.named_enum("lib.one.Enum", "lib_one_Enum");
    const TypeHandle payload_enum_type = types.named_enum("lib.one.Payload", "lib_one_Payload");
    const TypeHandle opaque_type = types.opaque_struct("lib.one.Opaque", "lib_one_Opaque");
    const TypeHandle nested_array_i16 = types.array(SEMA_TEST_NESTED_ARRAY_COUNT, array_i16);
    const TypeHandle array_void = types.array(SEMA_TEST_INVALID_ARRAY_COUNT, void_type);
    const TypeHandle array_opaque = types.array(SEMA_TEST_INVALID_ARRAY_COUNT, opaque_type);
    const TypeHandle overflowing_array = types.array(SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT, array_i16);
    const TypeHandle place_record_type = types.named_struct("root.PlaceRecord", "root_PlaceRecord", false);
    const TypeHandle ptr_place_record = types.pointer(PointerMutability::mut, place_record_type);
    const TypeHandle ref_nested_array_i16 = types.reference(PointerMutability::mut, nested_array_i16);
    const TypeHandle generic_param = types.generic_param(sema::generic_param_identity_from_text("layout.T"), "T");
    types.set_enum_underlying(enum_type, u16);
    types.set_enum_underlying(payload_enum_type, u8);
    types.set_enum_payload_layout(payload_enum_type, u64, sizeof(std::uint64_t), alignof(std::uint64_t));

    StructInfo record;
    record.name = checked_text(analyzer.state_.checked, "Record");
    record.name_id = intern_identifier(analyzer, "Record");
    record.module = module_id(1);
    record.type = record_type;
    record.fields = {
        struct_field_info(analyzer, "tag", module_id(1), u8),
        struct_field_info(analyzer, "value", module_id(1), u64),
    };
    analyzer.state_.checked.structs.emplace(semantic_module_key(analyzer, module_id(1), "Record"), record);

    StructInfo place_record;
    place_record.name = checked_text(analyzer.state_.checked, "PlaceRecord");
    place_record.name_id = intern_identifier(analyzer, "PlaceRecord");
    place_record.module = module_id(0);
    place_record.type = place_record_type;
    place_record.fields = {
        struct_field_info(analyzer, "field", module_id(0), place_record_type),
    };
    analyzer.state_.checked.structs.emplace(semantic_module_key(analyzer, module_id(0), "PlaceRecord"), place_record);

    EXPECT_EQ(analyzer.abi_size(INVALID_TYPE_HANDLE), SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(analyzer.abi_align(INVALID_TYPE_HANDLE), SEMA_TEST_ABI_MIN_ALIGNMENT);
    EXPECT_EQ(analyzer.abi_size(void_type), SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(analyzer.abi_align(void_type), SEMA_TEST_ABI_MIN_ALIGNMENT);
    EXPECT_EQ(analyzer.abi_size(bool_type), sema::SEMA_DEFAULT_TARGET_BOOL_SIZE);
    EXPECT_EQ(analyzer.abi_align(bool_type), sema::SEMA_DEFAULT_TARGET_BOOL_ALIGN);
    EXPECT_EQ(analyzer.abi_size(i8), sema::SEMA_DEFAULT_TARGET_I8_SIZE);
    EXPECT_EQ(analyzer.abi_size(u8), sema::SEMA_DEFAULT_TARGET_I8_SIZE);
    EXPECT_EQ(analyzer.abi_size(i16), sema::SEMA_DEFAULT_TARGET_I16_SIZE);
    EXPECT_EQ(analyzer.abi_size(u16), sema::SEMA_DEFAULT_TARGET_I16_SIZE);
    EXPECT_EQ(analyzer.abi_size(i32), sema::SEMA_DEFAULT_TARGET_I32_SIZE);
    EXPECT_EQ(analyzer.abi_size(u32), sema::SEMA_DEFAULT_TARGET_I32_SIZE);
    EXPECT_EQ(analyzer.abi_size(i64), sema::SEMA_DEFAULT_TARGET_I64_SIZE);
    EXPECT_EQ(analyzer.abi_size(u64), sema::SEMA_DEFAULT_TARGET_I64_SIZE);
    EXPECT_EQ(analyzer.abi_size(isize), sema::SEMA_DEFAULT_TARGET_POINTER_SIZE);
    EXPECT_EQ(analyzer.abi_size(usize), sema::SEMA_DEFAULT_TARGET_POINTER_SIZE);
    EXPECT_EQ(analyzer.abi_size(f32), sema::SEMA_DEFAULT_TARGET_F32_SIZE);
    EXPECT_EQ(analyzer.abi_size(f64), sema::SEMA_DEFAULT_TARGET_F64_SIZE);
    EXPECT_EQ(analyzer.abi_size(str), sema::SEMA_DEFAULT_TARGET_POINTER_SIZE + sema::SEMA_DEFAULT_TARGET_POINTER_SIZE);
    EXPECT_EQ(analyzer.abi_size(char_type), sema::SEMA_DEFAULT_TARGET_CHAR_SIZE);
    const sema::SemanticAnalyzerCore::TypeAbiLayout i32_layout = analyzer.abi_layout(i32);
    EXPECT_EQ(i32_layout.size, sema::SEMA_DEFAULT_TARGET_I32_SIZE);
    EXPECT_EQ(i32_layout.align, sema::SEMA_DEFAULT_TARGET_I32_ALIGN);
    EXPECT_EQ(analyzer.abi_align(i8), sema::SEMA_DEFAULT_TARGET_I8_ALIGN);
    EXPECT_EQ(analyzer.abi_align(i64), sema::SEMA_DEFAULT_TARGET_I64_ALIGN);
    EXPECT_EQ(analyzer.abi_align(isize), sema::SEMA_DEFAULT_TARGET_POINTER_ALIGN);
    EXPECT_EQ(analyzer.abi_align(usize), sema::SEMA_DEFAULT_TARGET_POINTER_ALIGN);
    EXPECT_EQ(analyzer.abi_align(f32), sema::SEMA_DEFAULT_TARGET_F32_ALIGN);
    EXPECT_EQ(analyzer.abi_align(f64), sema::SEMA_DEFAULT_TARGET_F64_ALIGN);
    EXPECT_EQ(analyzer.abi_align(str), sema::SEMA_DEFAULT_TARGET_POINTER_ALIGN);
    EXPECT_EQ(analyzer.abi_align(char_type), sema::SEMA_DEFAULT_TARGET_CHAR_ALIGN);
    EXPECT_EQ(analyzer.abi_size(ptr_i32), sema::SEMA_DEFAULT_TARGET_POINTER_SIZE);
    EXPECT_EQ(analyzer.abi_align(ptr_i32), sema::SEMA_DEFAULT_TARGET_POINTER_ALIGN);
    EXPECT_EQ(analyzer.abi_size(array_i16), SEMA_TEST_SMALL_ARRAY_COUNT * sema::SEMA_DEFAULT_TARGET_I16_SIZE);
    EXPECT_EQ(analyzer.abi_align(array_i16), sema::SEMA_DEFAULT_TARGET_I16_ALIGN);
    EXPECT_EQ(analyzer.abi_size(missing_struct), SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(analyzer.abi_align(missing_struct), SEMA_TEST_ABI_MIN_ALIGNMENT);
    EXPECT_EQ(analyzer.abi_size(record_type), SEMA_TEST_RECORD_ABI_SIZE);
    EXPECT_EQ(analyzer.abi_align(record_type), alignof(std::uint64_t));
    EXPECT_EQ(analyzer.abi_size(enum_type), sema::SEMA_DEFAULT_TARGET_I16_SIZE);
    EXPECT_EQ(analyzer.abi_align(enum_type), sema::SEMA_DEFAULT_TARGET_I16_ALIGN);
    EXPECT_GT(analyzer.abi_size(payload_enum_type), sema::SEMA_DEFAULT_TARGET_I8_SIZE);
    EXPECT_EQ(analyzer.abi_align(payload_enum_type), sema::SEMA_DEFAULT_TARGET_I64_ALIGN);
    EXPECT_EQ(analyzer.abi_size(opaque_type), SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(analyzer.abi_align(opaque_type), SEMA_TEST_ABI_MIN_ALIGNMENT);
    static_cast<void>(analyzer.abi_layout(place_record_type));

    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::cast, INVALID_TYPE_HANDLE, i32));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::cast, generic_param, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::cast, f64, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::pcast, const_ptr_i32, ptr_i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::bcast, i32, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::bcast, u32, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::bcast, char_type, u32));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::cast, u32, char_type));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::bcast, bool_type, i8));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::bcast, str, ptr_i32));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::ptr_addr, u32, ptr_i32));
    EXPECT_FALSE(analyzer.is_valid_storage_type(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(analyzer.is_valid_storage_type(void_type));
    EXPECT_FALSE(analyzer.is_valid_storage_type(opaque_type));
    EXPECT_TRUE(analyzer.is_valid_storage_type(char_type));
    EXPECT_TRUE(analyzer.is_valid_storage_type(nested_array_i16));
    EXPECT_FALSE(analyzer.is_valid_storage_type(array_void));
    EXPECT_FALSE(analyzer.is_valid_storage_type(array_opaque));
    EXPECT_FALSE(analyzer.is_valid_storage_type(overflowing_array));
    EXPECT_FALSE(analyzer.is_array_containing_value_type(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(analyzer.is_array_containing_value_type(i32));
    EXPECT_TRUE(analyzer.is_array_containing_value_type(nested_array_i16));
    EXPECT_FALSE(analyzer.can_assign(ref_i32, ref_u32, syntax::INVALID_EXPR_ID));
    EXPECT_FALSE(analyzer.can_assign(slice_i32, slice_u32, syntax::INVALID_EXPR_ID));
    EXPECT_TRUE(analyzer.check_m2_value_abi(INVALID_TYPE_HANDLE, sema::ValueAbiContext::parameter, {}));

    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "value", module_id(0), i32, true), diagnostics));
    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "ptr", module_id(0), ptr_place_record, true), diagnostics));
    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "record", module_id(0), place_record_type, true), diagnostics));
    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "array", module_id(0), nested_array_i16, true), diagnostics));
    EXPECT_TRUE(analyzer.state_.names.symbols.insert(
        indexed_symbol(analyzer, SymbolKind::local, "array_ref", module_id(0), ref_nested_array_i16, true),
        diagnostics));
    static_cast<void>(add_global_value(analyzer, module_id(1), "shared", i32, SymbolKind::local, true));

    EXPECT_TRUE(analyzer.is_place_expr(value_expr));
    EXPECT_FALSE(analyzer.is_place_expr(scoped_value_expr));
    EXPECT_FALSE(analyzer.is_place_expr(missing_scoped_value_expr));
    EXPECT_TRUE(analyzer.is_place_expr(field_id));
    EXPECT_TRUE(analyzer.is_place_expr(index_id));
    EXPECT_TRUE(analyzer.is_place_expr(nested_value_field_id));
    EXPECT_TRUE(analyzer.is_place_expr(nested_value_index_id));
    EXPECT_TRUE(analyzer.is_place_expr(deref_id));
    EXPECT_FALSE(analyzer.is_place_expr(not_id));
    EXPECT_FALSE(analyzer.is_place_expr(syntax::INVALID_EXPR_ID));
    EXPECT_TRUE(analyzer.is_writable_place(value_expr));
    EXPECT_FALSE(analyzer.is_writable_place(scoped_value_expr));
    EXPECT_FALSE(analyzer.is_writable_place(missing_scoped_value_expr));
    EXPECT_TRUE(analyzer.is_writable_place(field_id));
    EXPECT_TRUE(analyzer.is_writable_place(index_id));
    EXPECT_TRUE(analyzer.is_writable_place(value_field_id));
    EXPECT_TRUE(analyzer.is_writable_place(value_index_id));
    EXPECT_TRUE(analyzer.is_writable_place(nested_value_field_id));
    EXPECT_TRUE(analyzer.is_writable_place(nested_value_index_id));
    EXPECT_TRUE(analyzer.is_writable_place(array_ref_index_id));
    EXPECT_TRUE(analyzer.is_writable_place(deref_id));
    EXPECT_FALSE(analyzer.is_writable_place(not_id));
    EXPECT_FALSE(analyzer.is_writable_place(syntax::INVALID_EXPR_ID));
    const sema::SemanticAnalyzerCore::PlaceInfo invalid_deref_place =
        analyzer.analyze_place_info(invalid_deref_id, true);
    EXPECT_FALSE(invalid_deref_place.is_place);
    EXPECT_FALSE(diagnostics.diagnostics().empty());

    analyzer.state_.flow.current_module = syntax::INVALID_MODULE_ID;
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("missing", {})));
    analyzer.state_.flow.current_module = module_id(0);
    analyzer.ctx_.module.modules[0].imports.push_back(resolved_import(module_id(2), "one"));
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("one", {})));
    analyzer.ctx_.module.modules[0].imports.pop_back();
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("missing", {})));

    EXPECT_TRUE(analyzer.visible_modules(syntax::INVALID_MODULE_ID).empty());
    EXPECT_EQ(analyzer.visible_modules(module_id(SEMA_TEST_MISSING_MODULE_INDEX)).size(), 1U);
    EXPECT_TRUE(analyzer.module_export_modules(syntax::INVALID_MODULE_ID).empty());
    EXPECT_EQ(analyzer.module_export_modules(module_id(SEMA_TEST_MISSING_MODULE_INDEX)).size(), 1U);
    const auto& visible = analyzer.visible_modules(module_id(0));
    ASSERT_GE(visible.size(), 4U);
    EXPECT_EQ(analyzer.module_name(syntax::INVALID_MODULE_ID), "<unknown>");
    EXPECT_EQ(analyzer.qualified_name(syntax::INVALID_MODULE_ID, "Name"), "Name");
    EXPECT_EQ(analyzer.c_symbol_name(syntax::INVALID_MODULE_ID, "Name"), "Name");
}
TEST(CoreUnit, SemanticWhiteBoxTargetLayoutControlsPointerSizedIntegerSemantics)
{
    constexpr base::u64 SEMA_TEST_32BIT_POINTER_SIZE = 4;
    constexpr base::u64 SEMA_TEST_32BIT_POINTER_ALIGN = 4;

    syntax::AstModule module;
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle isize = types.builtin(BuiltinType::isize);
    const TypeHandle usize = types.builtin(BuiltinType::usize);
    const TypeHandle ref_i32 = types.reference(PointerMutability::const_, types.builtin(BuiltinType::i32));
    const TypeHandle slice_i32 = types.slice(PointerMutability::const_, types.builtin(BuiltinType::i32));

    analyzer.ctx_.options.target_layout.pointer_size = SEMA_TEST_32BIT_POINTER_SIZE;
    analyzer.ctx_.options.target_layout.pointer_align = SEMA_TEST_32BIT_POINTER_ALIGN;

    EXPECT_EQ(analyzer.abi_size(isize), SEMA_TEST_32BIT_POINTER_SIZE);
    EXPECT_EQ(analyzer.abi_size(usize), SEMA_TEST_32BIT_POINTER_SIZE);
    EXPECT_EQ(analyzer.abi_align(ref_i32), SEMA_TEST_32BIT_POINTER_ALIGN);
    EXPECT_EQ(analyzer.abi_size(slice_i32), SEMA_TEST_32BIT_POINTER_SIZE + SEMA_TEST_32BIT_POINTER_SIZE);
    EXPECT_TRUE(analyzer.integer_literal_fits_type(usize, "4294967295usize"));
    EXPECT_FALSE(analyzer.integer_literal_fits_type(usize, "4294967296usize"));
    EXPECT_TRUE(analyzer.negative_integer_literal_fits_type(isize, "2147483648isize"));
    EXPECT_FALSE(analyzer.negative_integer_literal_fits_type(isize, "2147483649isize"));
}
TEST(CoreUnit, SemanticWhiteBoxSyntaxTypeCacheDisabledDoesNotRead)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};
    const TypeId bool_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::bool_));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    const TypeHandle bool_type = analyzer.state_.checked.types.builtin(BuiltinType::bool_);
    analyzer.state_.checked.syntax_type_handles[bool_type_id.value] = bool_type;

    EXPECT_EQ(analyzer.cached_syntax_type(bool_type_id).value, bool_type.value);
    analyzer.state_.flow.current_side_tables.cache_syntax_types = false;
    EXPECT_FALSE(is_valid(analyzer.cached_syntax_type(bool_type_id)));
}
TEST(CoreUnit, SemanticWhiteBoxLiteralSuffixCapabilityAndStorageEdges)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
    };

    const ExprId negative_valid = push_integer_text(module, SEMA_TEST_NEGATIVE_I8_MIN_MAGNITUDE_SUFFIX);
    const ExprId negative_invalid_suffix = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_BAD_SUFFIX);
    const ExprId negative_mismatch = push_integer_text(module, SEMA_TEST_NEGATIVE_I8_MISMATCH_SUFFIX);
    const ExprId negative_overflow = push_integer_text(module, SEMA_TEST_NEGATIVE_I8_OVERFLOW_SUFFIX);
    const ExprId float_mismatch =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_LITERAL_MISMATCH_F32_SUFFIX);
    const ExprId void_value = push_name(module, "void_value");
    const ExprId shared_void_ref = push_unary(module, syntax::UnaryOp::address_of, void_value);
    const ExprId mut_void_ref = push_unary(module, syntax::UnaryOp::address_of_mut, void_value);
    const TypeId generic_type = module.push_type(named_node(SEMA_TEST_GENERIC_PARAM_NAME));
    const ExprId size_of_generic = module.push_cast_like_expr(
        syntax::ExprKind::size_of, {}, syntax::CastExprPayload{generic_type, syntax::INVALID_EXPR_ID});
    const ExprId lib_alias = push_name(module, "lib");
    const ExprId lib_type_member = push_field(module, lib_alias, "Thing");
    const ExprId lib_template_member = push_field(module, lib_alias, "Box");

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    prepare_expr_storage(analyzer, module);
    analyzer.state_.checked.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle i8 = types.builtin(BuiltinType::i8);
    const TypeHandle i16 = types.builtin(BuiltinType::i16);
    const TypeHandle f64 = types.builtin(BuiltinType::f64);
    const TypeHandle usize = types.builtin(BuiltinType::usize);

    EXPECT_TRUE(analyzer.negative_integer_literal_fits_type(i8, SEMA_TEST_NEGATIVE_I8_MIN_MAGNITUDE_SUFFIX));
    EXPECT_FALSE(analyzer.negative_integer_literal_fits_type(i16, SEMA_TEST_NEGATIVE_I8_MIN_MAGNITUDE_SUFFIX));
    EXPECT_FALSE(analyzer.negative_integer_literal_fits_type(i8, SEMA_TEST_INTEGER_LITERAL_BAD_SUFFIX));
    EXPECT_TRUE(types.same(analyzer.analyze_negative_integer_literal(
                               negative_valid, SEMA_TEST_NEGATIVE_I8_MIN_MAGNITUDE_SUFFIX, {}, INVALID_TYPE_HANDLE),
        i8));
    EXPECT_TRUE(types.same(analyzer.analyze_negative_integer_literal(
                               negative_invalid_suffix, SEMA_TEST_INTEGER_LITERAL_BAD_SUFFIX, {}, INVALID_TYPE_HANDLE),
        types.builtin(BuiltinType::i32)));
    EXPECT_TRUE(types.same(
        analyzer.analyze_negative_integer_literal(negative_mismatch, SEMA_TEST_NEGATIVE_I8_MISMATCH_SUFFIX, {}, i16),
        i8));
    EXPECT_TRUE(types.same(analyzer.analyze_negative_integer_literal(
                               negative_overflow, SEMA_TEST_NEGATIVE_I8_OVERFLOW_SUFFIX, {}, INVALID_TYPE_HANDLE),
        i8));
    EXPECT_TRUE(
        types.same(analyzer.analyze_float_literal(float_mismatch, SEMA_TEST_FLOAT_LITERAL_MISMATCH_F32_SUFFIX, {}, f64),
            types.builtin(BuiltinType::f32)));

    const TypeHandle generic =
        types.generic_param(sema::generic_param_identity_from_text("test.T"), SEMA_TEST_GENERIC_PARAM_NAME);
    sema::SemanticAnalyzerCore::GenericContext generic_context = analyzer.make_generic_context();
    const IdentId generic_id = intern_identifier(analyzer, SEMA_TEST_GENERIC_PARAM_NAME);
    generic_context.params.emplace(generic_id, generic);
    generic_context.param_identities.emplace(generic_id, sema::generic_param_identity_from_text("test.T"));
    analyzer.state_.flow.current_generic_context = &generic_context;
    EXPECT_FALSE(analyzer.generic_param_has_capability(generic, sema::CapabilityKind::ord));
    sema::SemanticAnalyzerCore::ExprView ordering_expr;
    ordering_expr.kind = syntax::ExprKind::binary;
    ordering_expr.binary_op = syntax::BinaryOp::less;
    EXPECT_TRUE(types.is_bool(analyzer.record_ordering_binary_expr(syntax::INVALID_EXPR_ID, ordering_expr, generic)));
    EXPECT_TRUE(
        types.same(analyzer.analyze_size_or_align_expr(size_of_generic, analyzer.expr_view(size_of_generic)), usize));
    analyzer.state_.flow.current_generic_context = nullptr;

    EXPECT_TRUE(analyzer.state_.names.symbols.insert(indexed_symbol(analyzer, SymbolKind::local, "void_value",
                                                         module_id(SEMA_TEST_ROOT_MODULE_INDEX), void_type, true),
        diagnostics));
    EXPECT_TRUE(types.is_reference(
        analyzer.analyze_unary_expr(shared_void_ref, analyzer.expr_view(shared_void_ref), INVALID_TYPE_HANDLE)));
    EXPECT_TRUE(types.is_reference(
        analyzer.analyze_unary_expr(mut_void_ref, analyzer.expr_view(mut_void_ref), INVALID_TYPE_HANDLE)));

    const TypeHandle lib_thing = types.named_struct("lib.Thing", "lib_Thing", false);
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Thing", lib_thing));
    sema::SemanticAnalyzerCore::GenericTemplateInfo generic_template =
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Box");
    const auto inserted_template =
        analyzer.state_.generics.struct_templates.emplace(generic_template.key, generic_template);
    ASSERT_TRUE(inserted_template.second);
    analyzer.index_generic_struct_template(inserted_template.first->second);
    EXPECT_FALSE(is_valid(analyzer.analyze_module_member_expr(
        lib_type_member, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), analyzer.expr_view(lib_type_member))));
    EXPECT_FALSE(is_valid(analyzer.analyze_module_member_expr(
        lib_template_member, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), analyzer.expr_view(lib_template_member))));

    std::string messages;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        messages += diagnostic.message;
        messages.push_back('\n');
    }
    EXPECT_NE(messages.find("invalid integer literal suffix"), std::string::npos);
    EXPECT_NE(messages.find("integer literal suffix type"), std::string::npos);
    EXPECT_NE(messages.find("integer literal out of range"), std::string::npos);
    EXPECT_NE(messages.find("float literal suffix type"), std::string::npos);
    EXPECT_NE(messages.find("generic type parameter cannot be queried by sizeof or alignof"), std::string::npos);
    EXPECT_NE(messages.find("reference requires a valid storage type"), std::string::npos);
}
TEST(CoreUnit, SemanticWhiteBoxIterativeTypeLayoutEdges)
{
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.flow.current_module = module_id(0);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle u64 = types.builtin(BuiltinType::u64);

    const TypeHandle max_array_u8 = types.array(SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT, u8);
    EXPECT_EQ(analyzer.abi_size(max_array_u8), SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT);
    EXPECT_EQ(analyzer.abi_align(max_array_u8), alignof(std::uint8_t));

    const TypeHandle overflow_array_u64 = types.array(SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT, u64);
    EXPECT_EQ(analyzer.abi_size(overflow_array_u64), SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT);
    EXPECT_EQ(analyzer.abi_align(overflow_array_u64), alignof(std::uint64_t));

    const TypeHandle overflow_struct_type = types.named_struct("OverflowStruct", "OverflowStruct", true);
    const TypeHandle empty_struct_type = types.named_struct("EmptyStruct", "EmptyStruct", false);
    StructInfo empty_struct;
    empty_struct.name = analyzer.state_.checked.intern_text("EmptyStruct");
    empty_struct.name_id = intern_identifier(analyzer, "EmptyStruct");
    empty_struct.module = module_id(0);
    empty_struct.type = empty_struct_type;
    analyzer.state_.checked.structs.emplace(semantic_module_key(analyzer, module_id(0), "EmptyStruct"), empty_struct);

    StructInfo overflow_struct;
    overflow_struct.name = analyzer.state_.checked.intern_text("OverflowStruct");
    overflow_struct.name_id = intern_identifier(analyzer, "OverflowStruct");
    overflow_struct.module = module_id(0);
    overflow_struct.type = overflow_struct_type;
    overflow_struct.fields = {
        struct_field_info(analyzer, "huge", module_id(0), max_array_u8),
        struct_field_info(analyzer, "tail", module_id(0), u64),
    };
    analyzer.state_.checked.structs.emplace(
        semantic_module_key(analyzer, module_id(0), "OverflowStruct"), overflow_struct);

    const TypeHandle overflow_enum_type = types.named_enum("OverflowEnum", "OverflowEnum");
    types.set_enum_underlying(overflow_enum_type, max_array_u8);
    EnumCaseInfo overflow_case;
    overflow_case.name = analyzer.state_.checked.intern_text("payload");
    overflow_case.name_id = intern_identifier(analyzer, "payload");
    overflow_case.case_name = analyzer.state_.checked.intern_text("payload");
    overflow_case.case_name_id = intern_identifier(analyzer, "payload");
    overflow_case.module = module_id(0);
    overflow_case.type = overflow_enum_type;
    overflow_case.payload_type = u64;
    analyzer.state_.checked.enum_cases.emplace(semantic_module_key(analyzer, module_id(0), "payload"), overflow_case);
    static_cast<void>(add_named_type(analyzer, module_id(0), "OverflowEnum", overflow_enum_type));

    analyzer.validate_type_layouts();
    EXPECT_GT(diagnostics.diagnostics().size(), 0U);
}
TEST(CoreUnit, SemanticWhiteBoxTypeResolverAndAbiFocusedEdges)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
    };

    const TypeId void_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::void_));
    const TypeId i32_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    syntax::TypeNode function_type;
    function_type.kind = syntax::TypeKind::function;
    function_type.function_is_variadic = true;
    function_type.function_params = {void_type_id};
    function_type.function_return = void_type_id;
    const TypeId invalid_function_type_id = module.push_type(function_type);

    syntax::TypeNode generic_arg_type = named_node(SEMA_TEST_GENERIC_PARAM_NAME);
    generic_arg_type.type_args = {i32_type_id};
    const TypeId generic_arg_type_id = module.push_type(generic_arg_type);

    syntax::TypeNode scoped_template_type = named_node("Box");
    scoped_template_type.scope_name = "lib";
    scoped_template_type.scope_name_id = module.intern_identifier("lib");
    scoped_template_type.name_id = module.intern_identifier("Box");
    const TypeId scoped_template_type_id = module.push_type(scoped_template_type);
    const TypeId plain_type_id = module.push_type(named_node("Plain"));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle u64 = types.builtin(BuiltinType::u64);

    sema::SemanticAnalyzerCore::GenericContext generic_context = analyzer.make_generic_context();
    const IdentId generic_id = intern_identifier(analyzer, SEMA_TEST_GENERIC_PARAM_NAME);
    generic_context.params.emplace(generic_id,
        types.generic_param(sema::generic_param_identity_from_text("test.T"), SEMA_TEST_GENERIC_PARAM_NAME));
    analyzer.state_.flow.current_generic_context = &generic_context;
    EXPECT_FALSE(is_valid(analyzer.resolve_type(generic_arg_type_id)));
    analyzer.state_.flow.current_generic_context = nullptr;

    sema::SemanticAnalyzerCore::GenericTemplateInfo box_template =
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Box");
    const auto inserted_box = analyzer.state_.generics.struct_templates.emplace(box_template.key, box_template);
    analyzer.index_generic_struct_template(inserted_box.first->second);

    EXPECT_TRUE(types.is_function(analyzer.resolve_type(invalid_function_type_id)));
    EXPECT_FALSE(is_valid(analyzer.resolve_type(scoped_template_type_id)));
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Plain", i32));
    EXPECT_TRUE(types.same(
        analyzer.resolve_named_type(syntax::INVALID_TYPE_ID, module.types[plain_type_id.value], false), i32));

    sema::TypeInfo invalid_builtin_info;
    invalid_builtin_info.kind = TypeKind::builtin;
    invalid_builtin_info.builtin = static_cast<BuiltinType>(SEMA_TEST_INVALID_BUILTIN_TYPE_VALUE);
    const TypeHandle invalid_builtin = TypeTableTestAccess::push(types, invalid_builtin_info);
    EXPECT_FALSE(analyzer.integer_literal_fits_type(invalid_builtin, SEMA_TEST_INTEGER_LITERAL_ONE));
    const sema::SemanticAnalyzerCore::TypeAbiLayout invalid_builtin_layout = analyzer.abi_layout(invalid_builtin);
    EXPECT_EQ(invalid_builtin_layout.size, SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(invalid_builtin_layout.align, SEMA_TEST_ABI_MIN_ALIGNMENT);

    sema::TypeInfo invalid_kind_info;
    invalid_kind_info.kind = static_cast<TypeKind>(SEMA_TEST_INVALID_SEMA_TYPE_KIND_VALUE);
    const TypeHandle invalid_kind = TypeTableTestAccess::push(types, invalid_kind_info);
    EXPECT_EQ(analyzer.abi_size(invalid_kind), SEMA_TEST_ABI_INVALID_SIZE);

    const TypeHandle stale_struct_type = types.named_struct("StaleStruct", "StaleStruct", false);
    StructInfo stale_struct;
    stale_struct.name = checked_text(analyzer.state_.checked, "StaleStruct");
    stale_struct.name_id = intern_identifier(analyzer, "StaleStruct");
    stale_struct.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    stale_struct.type = TypeHandle{stale_struct_type.value + SEMA_TEST_STALE_STRUCT_CACHE_OFFSET};
    analyzer.state_.types.struct_infos_by_type[stale_struct_type.value] = &stale_struct;
    EXPECT_EQ(analyzer.find_struct(stale_struct_type), nullptr);
    EXPECT_EQ(analyzer.abi_size(stale_struct_type), SEMA_TEST_ABI_INVALID_SIZE);

    const TypeHandle builtin_tuple = types.tuple({i32, u64});
    const TypeHandle builtin_struct_type = types.named_struct("BuiltinStruct", "BuiltinStruct", false);
    StructInfo builtin_struct;
    builtin_struct.name = checked_text(analyzer.state_.checked, "BuiltinStruct");
    builtin_struct.name_id = intern_identifier(analyzer, "BuiltinStruct");
    builtin_struct.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    builtin_struct.type = builtin_struct_type;
    builtin_struct.fields = {struct_field_info(analyzer, "value", module_id(0), i32)};
    analyzer.state_.checked.structs.emplace(
        semantic_module_key(analyzer, module_id(0), "BuiltinStruct"), builtin_struct);
    static_cast<void>(analyzer.abi_size(builtin_tuple));
    static_cast<void>(analyzer.abi_size(builtin_struct_type));

    const TypeHandle array_i32 = types.array(SEMA_TEST_ARRAY_SLICE_LENGTH, i32);
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::parameter, {}));
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::function_type_parameter, {}));
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::function_type_return, {}));
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::return_value, {}));
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::assignment, {}));
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::enum_payload, {}));
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::enum_payload_argument, {}));
    EXPECT_FALSE(analyzer.check_m2_value_abi(array_i32, sema::ValueAbiContext::argument, {}));

    const std::string messages = diagnostic_messages(diagnostics);
    EXPECT_NE(messages.find("generic type parameter cannot take type arguments"), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_FUNCTION_TYPE_PARAMETER_STORAGE), std::string::npos);
    EXPECT_NE(messages.find(sema::SEMA_VARIADIC_FUNCTION_TYPE_EXTERN_C_ONLY), std::string::npos);
    EXPECT_NE(messages.find("generic type Box requires type arguments"), std::string::npos);
}
TEST(CoreUnit, SemanticWhiteBoxRecordTypeAndAssociatedOwnerEdges)
{
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "one")};

    const TypeId u8_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::u8));
    const TypeId i32_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const ExprId hex_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_HEX_UPPER);
    const ExprId lower_hex_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_HEX_LOWER);
    const ExprId upper_hex_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_HEX_UPPER);
    const ExprId bin_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_BIN_LOWER);
    const ExprId upper_bin_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_BIN_UPPER);
    const ExprId invalid_digit_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_INVALID_BINARY);
    const ExprId invalid_char_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_INVALID_CHAR);
    const ExprId empty_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_EMPTY);
    const ExprId overflow_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_UNSIGNED_OVERFLOW);
    const ExprId signed_overflow_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_SIGNED_OVERFLOW);
    const ExprId suffixed_u8_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_U8_SUFFIX);
    const ExprId suffixed_i8_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_I8_SUFFIX);
    const ExprId invalid_suffix_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_INVALID_SUFFIX);
    const ExprId float_overflow_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_OVERFLOW_LITERAL);
    const ExprId separated_float_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_WITH_SEPARATOR_LITERAL);
    const ExprId invalid_float_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_INVALID_TRAILING_LITERAL);
    const ExprId suffixed_float_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_LITERAL_F32_SUFFIX);
    const ExprId invalid_suffix_float_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_LITERAL_INVALID_SUFFIX);

    syntax::TypeNode scoped_missing_alias_type = named_node("Missing");
    scoped_missing_alias_type.scope_name = "missing";
    const TypeId scoped_missing_alias_type_id = module.push_type(scoped_missing_alias_type);
    syntax::TypeNode scoped_choice_missing_args_type = named_node("Choice");
    scoped_choice_missing_args_type.scope_name = "one";
    const TypeId scoped_choice_missing_args_type_id = module.push_type(scoped_choice_missing_args_type);
    syntax::TypeNode invalid_primitive_type;
    invalid_primitive_type.kind = syntax::TypeKind::primitive;
    invalid_primitive_type.primitive = SEMA_TEST_INVALID_PRIMITIVE_KIND;
    const TypeId invalid_primitive_type_id = module.push_type(invalid_primitive_type);
    syntax::TypeNode scoped_opaque_type = named_node("ScopedOpaque");
    scoped_opaque_type.scope_name = "one";
    const TypeId scoped_opaque_type_id = module.push_type(scoped_opaque_type);

    syntax::ItemNode enum_item;
    enum_item.kind = syntax::ItemKind::enum_decl;
    enum_item.name = "Choice";
    enum_item.enum_base_type = u8_type_id;
    enum_item.enum_cases = {
        syntax::EnumCaseDecl{"none", syntax::INVALID_TYPE_ID, {}, SEMA_TEST_INTEGER_LITERAL_ONE, {}},
    };
    const syntax::ItemId enum_item_id = module.push_item(enum_item);
    module.item_modules[enum_item_id.value] = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer(module, diagnostics);
    analyzer.state_.checked.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
    analyzer.state_.checked.pattern_c_name_ids.assign(SEMA_TEST_PATTERN_TRACKED_COUNT, sema::INVALID_IDENT_ID);
    analyzer.state_.checked.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.flow.current_module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.state_.checked.types;
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle u16 = types.builtin(BuiltinType::u16);
    const TypeHandle u32 = types.builtin(BuiltinType::u32);
    const TypeHandle u64 = types.builtin(BuiltinType::u64);
    const TypeHandle i8 = types.builtin(BuiltinType::i8);
    const TypeHandle i16 = types.builtin(BuiltinType::i16);
    const TypeHandle i64 = types.builtin(BuiltinType::i64);
    const TypeHandle isize = types.builtin(BuiltinType::isize);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle scoped_opaque = types.opaque_struct("one.ScopedOpaque", "one_ScopedOpaque");

    EXPECT_TRUE(analyzer.can_assign(u8, u8, hex_expr));
    EXPECT_TRUE(analyzer.can_assign(u8, u8, lower_hex_expr));
    EXPECT_TRUE(analyzer.can_assign(u16, u16, upper_hex_expr));
    EXPECT_TRUE(analyzer.can_assign(u32, u32, bin_expr));
    EXPECT_TRUE(analyzer.can_assign(u64, u64, upper_bin_expr));
    EXPECT_TRUE(analyzer.can_assign(i8, i8, hex_expr));
    EXPECT_TRUE(analyzer.can_assign(i16, i16, bin_expr));
    EXPECT_TRUE(analyzer.can_assign(isize, isize, bin_expr));
    EXPECT_FALSE(analyzer.can_assign(u8, u8, invalid_digit_expr));
    EXPECT_FALSE(analyzer.can_assign(u8, u8, invalid_char_expr));
    EXPECT_FALSE(analyzer.can_assign(u8, u8, empty_expr));
    EXPECT_FALSE(analyzer.can_assign(u64, u64, overflow_expr));
    EXPECT_FALSE(analyzer.can_assign(i64, i64, signed_overflow_expr));
    EXPECT_TRUE(analyzer.can_assign(u8, u8, suffixed_u8_expr));
    EXPECT_FALSE(analyzer.can_assign(u16, u16, suffixed_u8_expr));
    EXPECT_FALSE(analyzer.can_assign(bool_type, u8, hex_expr));
    const auto analyze_integer_literal_id = [&](const ExprId expr, const TypeHandle expected) {
        const sema::SemanticAnalyzerCore::ExprView view = analyzer.expr_view(expr);
        return analyzer.analyze_integer_literal(expr, view.text, view.range, expected);
    };
    const auto analyze_float_literal_id = [&](const ExprId expr, const TypeHandle expected) {
        const sema::SemanticAnalyzerCore::ExprView view = analyzer.expr_view(expr);
        return analyzer.analyze_float_literal(expr, view.text, view.range, expected);
    };
    EXPECT_TRUE(types.same(analyze_integer_literal_id(suffixed_i8_expr, INVALID_TYPE_HANDLE), i8));
    EXPECT_TRUE(types.same(
        analyze_integer_literal_id(invalid_suffix_expr, INVALID_TYPE_HANDLE), types.builtin(BuiltinType::i32)));
    EXPECT_TRUE(types.same(analyze_float_literal_id(float_overflow_expr_id, types.builtin(BuiltinType::f32)),
        types.builtin(BuiltinType::f32)));
    EXPECT_TRUE(types.same(
        analyze_float_literal_id(separated_float_expr_id, INVALID_TYPE_HANDLE), types.builtin(BuiltinType::f64)));
    EXPECT_TRUE(types.same(analyze_float_literal_id(invalid_float_expr_id, types.builtin(BuiltinType::f64)),
        types.builtin(BuiltinType::f64)));
    EXPECT_TRUE(types.same(
        analyze_float_literal_id(suffixed_float_expr_id, INVALID_TYPE_HANDLE), types.builtin(BuiltinType::f32)));
    EXPECT_TRUE(types.same(
        analyze_float_literal_id(invalid_suffix_float_expr_id, INVALID_TYPE_HANDLE), types.builtin(BuiltinType::f64)));
    EXPECT_TRUE(types.is_str(types.builtin(BuiltinType::str)));

    const TypeHandle zero_align_enum = types.named_enum("ZeroAlign", "ZeroAlign");
    types.set_enum_underlying(zero_align_enum, u8);
    types.set_enum_payload_layout(
        zero_align_enum, u8, SEMA_TEST_ZERO_ALIGN_ENUM_PAYLOAD_SIZE, SEMA_TEST_ZERO_ALIGN_ENUM_PAYLOAD_ALIGN);
    EXPECT_GE(analyzer.abi_size(zero_align_enum), SEMA_TEST_ZERO_ALIGN_ENUM_PAYLOAD_SIZE);

    analyzer.record_pattern_c_name(syntax::INVALID_PATTERN_ID, "ignored");
    analyzer.record_pattern_c_name(syntax::PatternId{SEMA_TEST_PATTERN_FIRST_INDEX}, {});
    analyzer.record_pattern_case_name(syntax::INVALID_PATTERN_ID, "ignored");
    analyzer.record_pattern_case_name(syntax::PatternId{SEMA_TEST_PATTERN_FIRST_INDEX}, {});
    analyzer.merge_pattern_case_names(syntax::INVALID_PATTERN_ID, syntax::PatternId{SEMA_TEST_PATTERN_FIRST_INDEX});
    analyzer.state_.checked.pattern_case_name_ids[SEMA_TEST_PATTERN_SECOND_INDEX].insert(
        analyzer.state_.checked.intern_c_name("from_checked"));
    analyzer.merge_pattern_case_names(
        syntax::PatternId{SEMA_TEST_PATTERN_FIRST_INDEX}, syntax::PatternId{SEMA_TEST_PATTERN_SECOND_INDEX});
    EXPECT_TRUE(analyzer.state_.checked.pattern_case_name_ids[SEMA_TEST_PATTERN_FIRST_INDEX].contains(
        analyzer.state_.checked.intern_c_name("from_checked")));

    EXPECT_FALSE(is_valid(analyzer.resolve_associated_type_owner(syntax::INVALID_EXPR_ID, false)));

    const TypeHandle choice_type = types.named_enum("lib.one.Choice", "lib_one_Choice");
    types.set_enum_underlying(choice_type, u8);
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Choice", choice_type));
    static_cast<void>(
        add_named_type(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "ScopedOpaque", scoped_opaque));
    static_cast<void>(
        add_enum_case(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Choice_none", "none", choice_type));

    const ExprId import_alias_expr = push_name(analyzer.ctx_.module, "one");
    const ExprId scoped_enum_id = push_field(analyzer.ctx_.module, import_alias_expr, "Choice");
    analyzer.state_.checked.expr_types.resize(analyzer.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.resize(analyzer.ctx_.module.exprs.size(), sema::INVALID_IDENT_ID);
    const TypeHandle choice_i32 = analyzer.resolve_associated_type_owner(scoped_enum_id, false);
    EXPECT_TRUE(is_valid(choice_i32));

    const ExprId scoped_missing_type_id = push_field(analyzer.ctx_.module, import_alias_expr, "MissingScoped");
    analyzer.state_.checked.expr_types.resize(analyzer.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.resize(analyzer.ctx_.module.exprs.size(), sema::INVALID_IDENT_ID);
    EXPECT_FALSE(is_valid(analyzer.resolve_associated_type_owner(scoped_missing_type_id, false)));

    const TypeHandle opaque = types.opaque_struct("Opaque", "Opaque");
    analyzer.state_.checked.syntax_type_handles[i32_type_id.value] = opaque;
    EXPECT_TRUE(analyzer.state_.checked.types.same(analyzer.resolve_type(i32_type_id), opaque));
    analyzer.state_.checked.syntax_type_handles[i32_type_id.value] = INVALID_TYPE_HANDLE;
    EXPECT_TRUE(types.is_void(analyzer.resolve_type(invalid_primitive_type_id)));
    EXPECT_TRUE(types.same(analyzer.resolve_type(scoped_opaque_type_id), scoped_opaque));
    EXPECT_FALSE(is_valid(analyzer.resolve_type(scoped_missing_alias_type_id)));
    EXPECT_TRUE(types.same(analyzer.resolve_type(scoped_choice_missing_args_type_id), choice_type));

    const TypeHandle record_type = types.named_struct("Record", "Record", false);
    const TypeHandle array_record = types.named_struct("ArrayRecord", "ArrayRecord", true);
    types.set_record_contains_array(array_record, true);
    static_cast<void>(add_named_type(analyzer, module_id(0), "Record", record_type));
    static_cast<void>(add_global_value(analyzer, module_id(0), "array_value", array_record, SymbolKind::local));

    FunctionSignature static_method = indexed_function_signature(analyzer, "needs", module_id(0), bool_type);
    static_method.is_method = true;
    static_method.method_owner_type = record_type;
    static_cast<void>(add_method(analyzer, static_method, record_type));
    FunctionSignature needs_arg = indexed_function_signature(analyzer, "needs_arg", module_id(0), bool_type);
    needs_arg.is_method = true;
    needs_arg.method_owner_type = record_type;
    needs_arg.param_types = {u8};
    static_cast<void>(add_method(analyzer, needs_arg, record_type));
    FunctionSignature takes_array = indexed_function_signature(analyzer, "takes_array", module_id(0), bool_type);
    takes_array.is_method = true;
    takes_array.method_owner_type = record_type;
    takes_array.param_types = {array_record};
    static_cast<void>(add_method(analyzer, takes_array, record_type));

    const ExprId record_name = push_name(module, "Record");
    const ExprId static_method_field_id = push_field(module, record_name, "needs");
    const ExprId static_method_call_id = push_call(module, static_method_field_id);

    const ExprId missing_arg_field_id = push_field(module, record_name, "needs_arg");
    const ExprId missing_arg_call_id = push_call(module, missing_arg_field_id);

    const ExprId array_value = push_name(module, "array_value");
    const ExprId array_arg_field_id = push_field(module, record_name, "takes_array");
    const ExprId array_arg_call_id = push_call(module, array_arg_field_id, {array_value});

    const ExprId choice_name = push_name(module, "Choice");
    const ExprId none_field_id = push_field(module, choice_name, "none");
    const ExprId none_call_id = push_call(module, none_field_id);

    analyzer.state_.checked.expr_types.resize(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.state_.checked.expr_c_name_ids.resize(module.exprs.size(), sema::INVALID_IDENT_ID);
    static_cast<void>(analyzer.analyze_call_expr(
        static_method_call_id, analyzer.expr_view(static_method_call_id), INVALID_TYPE_HANDLE));
    static_cast<void>(
        analyzer.analyze_call_expr(missing_arg_call_id, analyzer.expr_view(missing_arg_call_id), INVALID_TYPE_HANDLE));
    static_cast<void>(
        analyzer.analyze_call_expr(array_arg_call_id, analyzer.expr_view(array_arg_call_id), INVALID_TYPE_HANDLE));
    static_cast<void>(analyzer.analyze_call_expr(none_call_id, analyzer.expr_view(none_call_id), choice_i32));
}
TEST(CoreUnit, SemanticWhiteBoxTypeTableUnknownDisplayFallbacks)
{
    sema::TypeTable builtin_table;
    const TypeHandle builtin_type = builtin_table.builtin(BuiltinType::i32);
    ASSERT_LT(builtin_type.value, TypeTableTestAccess::entries(builtin_table).size());
    TypeTableTestAccess::entries(builtin_table)[builtin_type.value].builtin = SEMA_TEST_INVALID_BUILTIN_TYPE;
    EXPECT_EQ(builtin_table.display_name(builtin_type), SEMA_TEST_UNKNOWN_TYPE_DISPLAY);

    sema::TypeTable kind_table;
    const TypeHandle kind_type = kind_table.builtin(BuiltinType::i32);
    ASSERT_LT(kind_type.value, TypeTableTestAccess::entries(kind_table).size());
    TypeTableTestAccess::entries(kind_table)[kind_type.value].kind = SEMA_TEST_INVALID_TYPE_KIND;
    EXPECT_EQ(kind_table.display_name(kind_type), SEMA_TEST_UNKNOWN_TYPE_DISPLAY);

    const TypeHandle out_of_range_type{static_cast<base::u32>(TypeTableTestAccess::entries(kind_table).size())};
    EXPECT_FALSE(kind_table.is_integer(out_of_range_type));
    EXPECT_FALSE(kind_table.is_float(out_of_range_type));
    EXPECT_FALSE(kind_table.is_bool(out_of_range_type));
    EXPECT_FALSE(kind_table.is_str(out_of_range_type));
    EXPECT_FALSE(kind_table.is_char(out_of_range_type));
    EXPECT_FALSE(kind_table.is_void(out_of_range_type));
    EXPECT_FALSE(kind_table.is_pointer(out_of_range_type));
    EXPECT_FALSE(kind_table.is_reference(out_of_range_type));
    EXPECT_FALSE(kind_table.is_array(out_of_range_type));
    EXPECT_FALSE(kind_table.is_slice(out_of_range_type));
    EXPECT_FALSE(kind_table.is_tuple(out_of_range_type));
    EXPECT_FALSE(kind_table.is_function(out_of_range_type));
    EXPECT_FALSE(kind_table.contains_array(out_of_range_type));
    EXPECT_EQ(kind_table.display_name(out_of_range_type), SEMA_TEST_INVALID_TYPE_DISPLAY);
    EXPECT_EQ(kind_table.c_name(out_of_range_type), "void");

    EXPECT_FALSE(kind_table.is_integer(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_float(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_bool(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_str(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_char(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_void(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_pointer(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_reference(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_array(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_slice(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_tuple(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_function(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.contains_array(INVALID_TYPE_HANDLE));

    EXPECT_FALSE(kind_table.is_integer(builtin_type));
    EXPECT_FALSE(kind_table.is_float(builtin_type));
    EXPECT_FALSE(kind_table.is_bool(builtin_type));
    EXPECT_FALSE(kind_table.is_str(builtin_type));
    EXPECT_FALSE(kind_table.is_char(builtin_type));
    EXPECT_FALSE(kind_table.is_void(builtin_type));
    EXPECT_FALSE(kind_table.is_pointer(builtin_type));
    EXPECT_FALSE(kind_table.is_reference(builtin_type));
    EXPECT_FALSE(kind_table.is_array(builtin_type));
    EXPECT_FALSE(kind_table.is_slice(builtin_type));
    EXPECT_FALSE(kind_table.is_tuple(builtin_type));
    EXPECT_FALSE(kind_table.is_function(builtin_type));
    EXPECT_FALSE(kind_table.contains_array(builtin_type));

    sema::TypeTable storage_table;
    const TypeHandle i32 = storage_table.builtin(BuiltinType::i32);
    const TypeHandle bool_type = storage_table.builtin(BuiltinType::bool_);
    const TypeHandle pointer = storage_table.pointer(PointerMutability::const_, i32);
    const TypeHandle reference = storage_table.reference(PointerMutability::mut, bool_type);
    const std::array<std::string_view, 2> empty_origin_names{"", ""};
    const TypeHandle empty_origin_reference =
        storage_table.reference(PointerMutability::const_, i32, empty_origin_names);
    const TypeHandle plain_reference = storage_table.reference(PointerMutability::const_, i32);
    const TypeHandle array = storage_table.array(4, i32);
    const TypeHandle slice = storage_table.slice(PointerMutability::const_, i32);
    const TypeHandle tuple = storage_table.tuple({i32, bool_type});
    const TypeHandle function = storage_table.function(sema::FunctionCallConv::aurex, false, {i32, pointer}, bool_type);
    const TypeHandle unnamed_struct = storage_table.named_struct("NoCName", "", false);
    const std::array<TypeHandle, 1> span_function_params{i32};
    const TypeHandle span_function = storage_table.function(
        sema::FunctionCallConv::aurex, true, std::span<const TypeHandle>(span_function_params), bool_type);
    const TypeHandle generic = storage_table.generic_param(sema::generic_param_identity_from_text("test.T"), "T");
    EXPECT_FALSE(is_valid(storage_table.generic_param(sema::INVALID_GENERIC_PARAM_IDENTITY, "Invalid")));
    sema::TypeInfo invalid_generic_info;
    invalid_generic_info.kind = TypeKind::generic_param;
    invalid_generic_info.generic_identity = sema::INVALID_GENERIC_PARAM_IDENTITY;
    const TypeHandle invalid_generic = TypeTableTestAccess::push(storage_table, invalid_generic_info);
    EXPECT_LT(invalid_generic.value, TypeTableTestAccess::entries(storage_table).size());
    storage_table.set_generic_instance(tuple, "tuple.origin", {generic});
    storage_table.set_record_contains_array(tuple, true);
    storage_table.set_enum_underlying(function, i32);
    storage_table.set_enum_payload_layout(function, array, 8, 4);
    EXPECT_EQ(empty_origin_reference.value, plain_reference.value);
    EXPECT_EQ(storage_table.display_name("Plain", std::span<const TypeHandle>{}), "Plain");
    EXPECT_EQ(storage_table.c_name(unnamed_struct), "NoCName");

    sema::TypeTable* const storage_table_alias = &storage_table;
    storage_table = *storage_table_alias;
    EXPECT_TRUE(storage_table.is_tuple(tuple));
    storage_table = std::move(*storage_table_alias);
    EXPECT_TRUE(storage_table.is_function(function));

    sema::TypeTable copied_table(storage_table);
    EXPECT_EQ(copied_table.display_name(pointer), "*const i32");
    EXPECT_TRUE(copied_table.is_reference(reference));
    EXPECT_TRUE(copied_table.is_slice(slice));
    EXPECT_TRUE(copied_table.is_function(span_function));
    EXPECT_LT(invalid_generic.value, TypeTableTestAccess::entries(copied_table).size());
    sema::TypeTable assigned_table;
    assigned_table = storage_table;
    EXPECT_TRUE(assigned_table.contains_array(tuple));
    sema::TypeTable moved_table(std::move(copied_table));
    EXPECT_EQ(moved_table.get(function).enum_payload_align, 4U);
    sema::TypeTable move_assigned_table;
    move_assigned_table = std::move(assigned_table);
    EXPECT_TRUE(move_assigned_table.is_array(array));
}
} // namespace aurex::test
