#pragma once

#include <aurex/frontend/syntax/ast/nodes.hpp>
#include <aurex/frontend/syntax/core/token.hpp>

#include <cstdint>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

namespace aurex::syntax {

inline constexpr std::string_view SYNTAX_ITEM_PAYLOAD_ID_CONTEXT = "syntax item payload id";
inline constexpr std::string_view SYNTAX_ITEM_NODE_ID_CONTEXT = "syntax item node id";

enum class BorrowContractSelectorKind : base::u8 {
    parameter,
    self,
    static_,
    unknown,
};

struct BorrowContractSelectorDecl {
    BorrowContractSelectorKind kind = BorrowContractSelectorKind::parameter;
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    base::SourceRange range{};
};

struct BorrowContractDecl {
    std::vector<BorrowContractSelectorDecl> return_selectors;
    base::SourceRange range{};
    bool present = false;
};

struct DeriveDecl {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    base::SourceRange range{};
};

enum class AttributeTokenTreeGroupKind : base::u8 {
    none,
    paren,
    bracket,
    brace,
};

struct AttributeTokenDecl {
    TokenKind kind = TokenKind::invalid;
    std::string_view text;
    base::SourceRange range{};
    base::u32 depth = 0;
    AttributeTokenTreeGroupKind group = AttributeTokenTreeGroupKind::none;
};

struct AttributeDecl {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    base::SourceRange range{};
    base::SourceRange token_tree_range{};
    AstArenaVector<AttributeTokenDecl> token_tree;
    bool has_token_tree = false;
};

enum class MacroDeclKind : base::u8 {
    declarative,
    derive,
    compile_time,
};

struct FieldDecl {
    std::string_view name;
    TypeId type = INVALID_TYPE_ID;
    base::SourceRange range{};
    Visibility visibility = Visibility::private_;
    IdentId name_id = INVALID_IDENT_ID;
};

struct EnumCaseDecl {
    std::string_view name;
    TypeId payload_type = INVALID_TYPE_ID;
    AstArenaVector<TypeId> payload_types;
    std::string_view value_text;
    base::SourceRange range{};
    IdentId name_id = INVALID_IDENT_ID;
};

struct TraitSupertraitDecl {
    TypeId trait_type = INVALID_TYPE_ID;
    base::SourceRange range{};
    base::u32 ordinal = 0;
};

enum class ItemKind {
    const_decl,
    type_alias,
    struct_decl,
    enum_decl,
    opaque_struct_decl,
    trait_decl,
    fn_decl,
    extern_block,
    impl_block,
    macro_decl,
};

struct ItemNode {
    ItemKind kind = ItemKind::fn_decl;
    base::SourceRange range{};
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    std::vector<GenericParamDecl> generic_params;
    std::vector<GenericConstraintDecl> where_constraints;
    Visibility visibility = Visibility::private_;
    TypeId const_type = INVALID_TYPE_ID;
    ExprId const_value = INVALID_EXPR_ID;
    TypeId alias_type = INVALID_TYPE_ID;
    std::vector<FieldDecl> fields;
    TypeId enum_base_type = INVALID_TYPE_ID;
    std::vector<EnumCaseDecl> enum_cases;
    std::vector<ParamDecl> params;
    TypeId return_type = INVALID_TYPE_ID;
    StmtId body = INVALID_STMT_ID;
    TypeId impl_type = INVALID_TYPE_ID;
    TypeId trait_type = INVALID_TYPE_ID;
    bool is_export_c = false;
    bool is_extern_c = false;
    bool is_unsafe = false;
    bool is_variadic = false;
    bool is_prototype = false;
    bool is_trait_default_method = false;
    std::string_view abi_name;
    BorrowContractDecl borrow_contract;
    std::vector<AttributeDecl> attributes;
    std::vector<DeriveDecl> derives;
    MacroDeclKind macro_kind = MacroDeclKind::declarative;
    AstArenaVector<AttributeTokenDecl> macro_body_tokens;
    base::SourceRange macro_body_range{};
    base::u64 macro_match_clause_count = 0;
    bool macro_body_balanced = false;
    std::vector<TraitSupertraitDecl> trait_supertraits;
    std::vector<ItemId> trait_items;
    std::vector<ItemId> extern_items;
    std::vector<ItemId> impl_items;
};

struct ItemNodeHeader {
    base::SourceRange range{};
    base::u32 payload = UINT32_MAX;
    base::u8 kind = static_cast<base::u8>(ItemKind::fn_decl);
    base::u8 visibility = static_cast<base::u8>(Visibility::private_);
    base::u8 flags = 0;
};

struct ConstItemPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<AttributeDecl> attributes;
    AstArenaVector<DeriveDecl> derives;
    TypeId type = INVALID_TYPE_ID;
    ExprId value = INVALID_EXPR_ID;
};

struct TypeAliasItemPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<AttributeDecl> attributes;
    AstArenaVector<DeriveDecl> derives;
    AstArenaVector<GenericParamDecl> generic_params;
    AstArenaVector<GenericConstraintDecl> where_constraints;
    TypeId target = INVALID_TYPE_ID;
    TypeId impl_type = INVALID_TYPE_ID;
    TypeId trait_type = INVALID_TYPE_ID;
};

struct StructItemPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<AttributeDecl> attributes;
    AstArenaVector<DeriveDecl> derives;
    AstArenaVector<GenericParamDecl> generic_params;
    AstArenaVector<GenericConstraintDecl> where_constraints;
    AstArenaVector<FieldDecl> fields;
};

struct EnumItemPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<AttributeDecl> attributes;
    AstArenaVector<DeriveDecl> derives;
    AstArenaVector<GenericParamDecl> generic_params;
    AstArenaVector<GenericConstraintDecl> where_constraints;
    TypeId base_type = INVALID_TYPE_ID;
    AstArenaVector<EnumCaseDecl> cases;
};

struct OpaqueStructItemPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<AttributeDecl> attributes;
    AstArenaVector<DeriveDecl> derives;
};

struct TraitItemPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<AttributeDecl> attributes;
    AstArenaVector<DeriveDecl> derives;
    AstArenaVector<GenericParamDecl> generic_params;
    AstArenaVector<TraitSupertraitDecl> supertraits;
    AstArenaVector<GenericConstraintDecl> where_constraints;
    AstArenaVector<ItemId> items;
};

struct FunctionItemPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<AttributeDecl> attributes;
    AstArenaVector<DeriveDecl> derives;
    AstArenaVector<GenericParamDecl> generic_params;
    AstArenaVector<GenericConstraintDecl> where_constraints;
    AstArenaVector<ParamDecl> params;
    TypeId return_type = INVALID_TYPE_ID;
    StmtId body = INVALID_STMT_ID;
    TypeId impl_type = INVALID_TYPE_ID;
    TypeId trait_type = INVALID_TYPE_ID;
    std::string_view abi_name;
    AstArenaVector<BorrowContractSelectorDecl> borrow_return_selectors;
    base::SourceRange borrow_contract_range{};
    bool has_borrow_contract = false;
};

struct ExternBlockItemPayload {
    AstArenaVector<AttributeDecl> attributes;
    AstArenaVector<DeriveDecl> derives;
    AstArenaVector<ItemId> items;
};

struct ImplBlockItemPayload {
    AstArenaVector<AttributeDecl> attributes;
    AstArenaVector<DeriveDecl> derives;
    AstArenaVector<GenericParamDecl> generic_params;
    AstArenaVector<GenericConstraintDecl> where_constraints;
    TypeId impl_type = INVALID_TYPE_ID;
    TypeId trait_type = INVALID_TYPE_ID;
    AstArenaVector<ItemId> items;
};

struct MacroItemPayload {
    std::string_view name;
    IdentId name_id = INVALID_IDENT_ID;
    AstArenaVector<AttributeDecl> attributes;
    AstArenaVector<DeriveDecl> derives;
    AstArenaVector<AttributeTokenDecl> body_tokens;
    base::SourceRange body_range{};
    base::u64 match_clause_count = 0;
    base::u8 macro_kind = static_cast<base::u8>(MacroDeclKind::declarative);
    bool body_balanced = false;
};

struct ItemNodePayloadArena {
    ItemNodePayloadArena() = default;

    explicit ItemNodePayloadArena(base::BumpAllocator& arena)
        : consts(base::BumpAllocatorAdapter<ConstItemPayload>{arena}),
          type_aliases(base::BumpAllocatorAdapter<TypeAliasItemPayload>{arena}),
          structs(base::BumpAllocatorAdapter<StructItemPayload>{arena}),
          enums(base::BumpAllocatorAdapter<EnumItemPayload>{arena}),
          opaque_structs(base::BumpAllocatorAdapter<OpaqueStructItemPayload>{arena}),
          traits(base::BumpAllocatorAdapter<TraitItemPayload>{arena}),
          functions(base::BumpAllocatorAdapter<FunctionItemPayload>{arena}),
          extern_blocks(base::BumpAllocatorAdapter<ExternBlockItemPayload>{arena}),
          impl_blocks(base::BumpAllocatorAdapter<ImplBlockItemPayload>{arena}),
          macros(base::BumpAllocatorAdapter<MacroItemPayload>{arena}),
          unknowns(base::BumpAllocatorAdapter<ItemNode>{arena})
    {
    }

    void swap(ItemNodePayloadArena& other) noexcept
    {
        this->consts.swap(other.consts);
        this->type_aliases.swap(other.type_aliases);
        this->structs.swap(other.structs);
        this->enums.swap(other.enums);
        this->opaque_structs.swap(other.opaque_structs);
        this->traits.swap(other.traits);
        this->functions.swap(other.functions);
        this->extern_blocks.swap(other.extern_blocks);
        this->impl_blocks.swap(other.impl_blocks);
        this->macros.swap(other.macros);
        this->unknowns.swap(other.unknowns);
    }

    AstArenaVector<ConstItemPayload> consts;
    AstArenaVector<TypeAliasItemPayload> type_aliases;
    AstArenaVector<StructItemPayload> structs;
    AstArenaVector<EnumItemPayload> enums;
    AstArenaVector<OpaqueStructItemPayload> opaque_structs;
    AstArenaVector<TraitItemPayload> traits;
    AstArenaVector<FunctionItemPayload> functions;
    AstArenaVector<ExternBlockItemPayload> extern_blocks;
    AstArenaVector<ImplBlockItemPayload> impl_blocks;
    AstArenaVector<MacroItemPayload> macros;
    AstArenaVector<ItemNode> unknowns;
};

class ItemNodeList final {
public:
    ItemNodeList();
    ItemNodeList(const ItemNodeList& other);
    ItemNodeList& operator=(const ItemNodeList& other);
    ItemNodeList(ItemNodeList&& other) noexcept;
    ItemNodeList& operator=(ItemNodeList&& other) noexcept;
    ~ItemNodeList();

    [[nodiscard]] base::usize size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] ItemKind kind(base::usize index) const noexcept;
    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_used_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;
    [[nodiscard]] base::SourceRange range(base::usize index) const noexcept;
    [[nodiscard]] Visibility visibility(base::usize index) const noexcept;

    template <typename T>
    [[nodiscard]] AstArenaVector<T> make_list()
    {
        return make_ast_arena_vector<T>(*this->arena_);
    }

    void reserve(base::usize size);
    void reserve_headers(base::usize size);
    void push_back(ItemNode node);
    [[nodiscard]] ItemId append(ItemNode node);
    void set(base::usize index, ItemNode node);
    void set_range_begin(base::usize index, base::usize begin);
    void set_visibility(base::usize index, Visibility visibility);
    [[nodiscard]] ItemNode take(base::usize index);
    [[nodiscard]] ItemNode operator[](base::usize index) const;
    [[nodiscard]] const ItemNode* ptr(base::usize index) const;

private:
    static constexpr base::u8 ITEM_NODE_FLAG_EXPORT_C = 1U << 0U;
    static constexpr base::u8 ITEM_NODE_FLAG_EXTERN_C = 1U << 1U;
    static constexpr base::u8 ITEM_NODE_FLAG_UNSAFE = 1U << 2U;
    static constexpr base::u8 ITEM_NODE_FLAG_VARIADIC = 1U << 3U;
    static constexpr base::u8 ITEM_NODE_FLAG_PROTOTYPE = 1U << 4U;
    static constexpr base::u8 ITEM_NODE_FLAG_TRAIT_DEFAULT_METHOD = 1U << 5U;

    [[nodiscard]] static base::u8 pack_kind(ItemKind kind) noexcept;
    [[nodiscard]] static base::u8 pack_visibility(Visibility visibility) noexcept;
    [[nodiscard]] static bool has_flag(base::u8 flags, base::u8 flag) noexcept;
    [[nodiscard]] static base::u8 pack_flags(const ItemNode& node) noexcept;

    template <typename T, typename Allocator>
    [[nodiscard]] AstArenaVector<T> copy_list(const std::vector<T, Allocator>& values)
    {
        return copy_ast_arena_vector(*this->arena_, values);
    }

    template <typename T>
    [[nodiscard]] AstArenaVector<T> copy_or_move_list(AstArenaVector<T>&& values)
    {
        return move_or_copy_ast_arena_vector(*this->arena_, std::move(values));
    }

    template <typename T, typename Allocator>
    [[nodiscard]] AstArenaVector<T> copy_or_move_list(std::vector<T, Allocator>&& values)
    {
        return this->copy_list(values);
    }

    [[nodiscard]] GenericConstraintDecl copy_or_move_generic_constraint(GenericConstraintDecl&& constraint);
    [[nodiscard]] AttributeDecl copy_or_move_attribute(AttributeDecl&& attribute);
    [[nodiscard]] DeriveDecl copy_or_move_derive(DeriveDecl&& derive);

    template <typename Allocator>
    [[nodiscard]] AstArenaVector<GenericConstraintDecl> copy_or_move_generic_constraints(
        std::vector<GenericConstraintDecl, Allocator>&& constraints)
    {
        AstArenaVector<GenericConstraintDecl> copy = make_ast_arena_vector<GenericConstraintDecl>(*this->arena_);
        copy.reserve(constraints.size());
        for (GenericConstraintDecl& constraint : constraints) {
            copy.push_back(this->copy_or_move_generic_constraint(std::move(constraint)));
        }
        return copy;
    }

    template <typename Allocator>
    [[nodiscard]] AstArenaVector<AttributeDecl> copy_or_move_attributes(
        std::vector<AttributeDecl, Allocator>&& attributes)
    {
        AstArenaVector<AttributeDecl> copy = make_ast_arena_vector<AttributeDecl>(*this->arena_);
        copy.reserve(attributes.size());
        for (AttributeDecl& attribute : attributes) {
            copy.push_back(this->copy_or_move_attribute(std::move(attribute)));
        }
        return copy;
    }

    template <typename Allocator>
    [[nodiscard]] AstArenaVector<DeriveDecl> copy_or_move_derives(std::vector<DeriveDecl, Allocator>&& derives)
    {
        AstArenaVector<DeriveDecl> copy = make_ast_arena_vector<DeriveDecl>(*this->arena_);
        copy.reserve(derives.size());
        for (DeriveDecl& derive : derives) {
            copy.push_back(this->copy_or_move_derive(std::move(derive)));
        }
        return copy;
    }

    [[nodiscard]] EnumCaseDecl copy_or_move_enum_case(EnumCaseDecl&& enum_case);
    [[nodiscard]] BorrowContractDecl copy_or_move_borrow_contract(BorrowContractDecl&& contract);

    template <typename Allocator>
    [[nodiscard]] AstArenaVector<EnumCaseDecl> copy_or_move_enum_cases(std::vector<EnumCaseDecl, Allocator>&& cases)
    {
        AstArenaVector<EnumCaseDecl> copy = make_ast_arena_vector<EnumCaseDecl>(*this->arena_);
        copy.reserve(cases.size());
        for (EnumCaseDecl& enum_case : cases) {
            copy.push_back(this->copy_or_move_enum_case(std::move(enum_case)));
        }
        return copy;
    }

    [[nodiscard]] GenericConstraintDecl detach_generic_constraint(const GenericConstraintDecl& constraint) const;
    [[nodiscard]] AttributeDecl detach_attribute(const AttributeDecl& attribute) const;
    [[nodiscard]] DeriveDecl detach_derive(const DeriveDecl& derive) const;

    template <typename Allocator>
    [[nodiscard]] std::vector<GenericConstraintDecl> detach_generic_constraints(
        const std::vector<GenericConstraintDecl, Allocator>& constraints) const
    {
        std::vector<GenericConstraintDecl> copy;
        copy.reserve(constraints.size());
        for (const GenericConstraintDecl& constraint : constraints) {
            copy.push_back(this->detach_generic_constraint(constraint));
        }
        return copy;
    }

    template <typename Allocator>
    [[nodiscard]] std::vector<AttributeDecl> detach_attributes(
        const std::vector<AttributeDecl, Allocator>& attributes) const
    {
        std::vector<AttributeDecl> copy;
        copy.reserve(attributes.size());
        for (const AttributeDecl& attribute : attributes) {
            copy.push_back(this->detach_attribute(attribute));
        }
        return copy;
    }

    template <typename Allocator>
    [[nodiscard]] std::vector<DeriveDecl> detach_derives(const std::vector<DeriveDecl, Allocator>& derives) const
    {
        std::vector<DeriveDecl> copy;
        copy.reserve(derives.size());
        for (const DeriveDecl& derive : derives) {
            copy.push_back(this->detach_derive(derive));
        }
        return copy;
    }

    [[nodiscard]] EnumCaseDecl detach_enum_case(const EnumCaseDecl& enum_case) const;

    template <typename Allocator>
    [[nodiscard]] std::vector<EnumCaseDecl> detach_enum_cases(const std::vector<EnumCaseDecl, Allocator>& cases) const
    {
        std::vector<EnumCaseDecl> copy;
        copy.reserve(cases.size());
        for (const EnumCaseDecl& enum_case : cases) {
            copy.push_back(this->detach_enum_case(enum_case));
        }
        return copy;
    }

    [[nodiscard]] base::u32 store_payload(ItemNode node);
    void load_header(const ItemNodeHeader& header, ItemNode& node) const noexcept;
    [[nodiscard]] ItemNode load(base::usize index) const;
    [[nodiscard]] ItemNode load_moved(base::usize index);
    [[nodiscard]] const ItemNode& materialized(base::usize index) const;
    void ensure_materialized_capacity(base::usize size) const;
    void invalidate_materialized(base::usize index) const;

    template <typename T>
    [[nodiscard]] base::u32 push_payload(AstArenaVector<T>& payloads, T payload)
    {
        const base::u32 index = base::checked_u32(payloads.size(), SYNTAX_ITEM_PAYLOAD_ID_CONTEXT);
        payloads.push_back(std::move(payload));
        return index;
    }

    void copy_from(const ItemNodeList& other);
    void swap(ItemNodeList& other) noexcept;

    std::unique_ptr<base::BumpAllocator> arena_;
    AstArenaVector<ItemNodeHeader> headers_;
    ItemNodePayloadArena payloads_;
    mutable std::deque<ItemNode> materialized_;
    mutable std::vector<bool> materialized_valid_;
};

} // namespace aurex::syntax
