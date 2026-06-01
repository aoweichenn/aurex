#include <aurex/syntax/ast/item_nodes.hpp>

#include <cstdint>
#include <memory>
#include <utility>

namespace aurex::syntax {

ItemNodeList::ItemNodeList()
    : arena_(std::make_unique<base::BumpAllocator>()),
      headers_(base::BumpAllocatorAdapter<ItemNodeHeader>{*this->arena_}), payloads_(*this->arena_)
{
}

ItemNodeList::ItemNodeList(const ItemNodeList& other) : ItemNodeList()
{
    this->copy_from(other);
}

ItemNodeList& ItemNodeList::operator=(const ItemNodeList& other)
{
    if (this == &other) {
        return *this;
    }
    ItemNodeList copy(other);
    *this = std::move(copy);
    return *this;
}

ItemNodeList::ItemNodeList(ItemNodeList&& other) noexcept
    : arena_(std::move(other.arena_)), headers_(std::move(other.headers_)), payloads_(std::move(other.payloads_))
{
    other.headers_ = AstArenaVector<ItemNodeHeader>{};
    other.payloads_ = ItemNodePayloadArena{};
    other.materialized_.clear();
    other.materialized_valid_.clear();
}

ItemNodeList& ItemNodeList::operator=(ItemNodeList&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

ItemNodeList::~ItemNodeList() = default;

base::usize ItemNodeList::size() const noexcept
{
    return this->headers_.size();
}

bool ItemNodeList::empty() const noexcept
{
    return this->headers_.empty();
}

ItemKind ItemNodeList::kind(const base::usize index) const noexcept
{
    return static_cast<ItemKind>(this->headers_[index].kind);
}

base::usize ItemNodeList::arena_bytes() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
}

base::usize ItemNodeList::arena_used_bytes() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->used_bytes();
}

base::usize ItemNodeList::arena_blocks() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->block_count();
}

base::SourceRange ItemNodeList::range(const base::usize index) const noexcept
{
    return this->headers_[index].range;
}

Visibility ItemNodeList::visibility(const base::usize index) const noexcept
{
    return static_cast<Visibility>(this->headers_[index].visibility);
}

void ItemNodeList::reserve(const base::usize size)
{
    this->reserve_headers(size);
    const base::usize primary = ast_reserve_fraction(size, SYNTAX_AST_RESERVE_PRIMARY_PAYLOAD_DIVISOR);
    const base::usize secondary = ast_reserve_fraction(size, SYNTAX_AST_RESERVE_SECONDARY_PAYLOAD_DIVISOR);
    const base::usize rare = ast_reserve_fraction(size, SYNTAX_AST_RESERVE_RARE_PAYLOAD_DIVISOR);
    this->payloads_.consts.reserve(secondary);
    this->payloads_.type_aliases.reserve(secondary);
    this->payloads_.structs.reserve(primary);
    this->payloads_.enums.reserve(primary);
    this->payloads_.opaque_structs.reserve(rare);
    this->payloads_.traits.reserve(secondary);
    this->payloads_.functions.reserve(primary);
    this->payloads_.extern_blocks.reserve(rare);
    this->payloads_.impl_blocks.reserve(secondary);
    this->payloads_.unknowns.reserve(rare);
}

void ItemNodeList::reserve_headers(const base::usize size)
{
    this->headers_.reserve(size);
}

void ItemNodeList::push_back(ItemNode node)
{
    static_cast<void>(this->append(std::move(node)));
}

ItemId ItemNodeList::append(ItemNode node)
{
    const ItemId id{base::checked_u32(this->headers_.size(), SYNTAX_ITEM_NODE_ID_CONTEXT)};
    ItemNodeHeader header;
    header.kind = pack_kind(node.kind);
    header.range = node.range;
    header.visibility = pack_visibility(node.visibility);
    header.flags = pack_flags(node);
    header.payload = this->store_payload(std::move(node));
    this->headers_.push_back(header);
    return id;
}

void ItemNodeList::set(const base::usize index, ItemNode node)
{
    ItemNodeHeader header;
    header.kind = pack_kind(node.kind);
    header.range = node.range;
    header.visibility = pack_visibility(node.visibility);
    header.flags = pack_flags(node);
    header.payload = this->store_payload(std::move(node));
    this->headers_[index] = header;
    this->invalidate_materialized(index);
}

void ItemNodeList::set_range_begin(const base::usize index, const base::usize begin)
{
    this->headers_[index].range.begin = begin;
    this->invalidate_materialized(index);
}

void ItemNodeList::set_visibility(const base::usize index, const Visibility visibility)
{
    this->headers_[index].visibility = pack_visibility(visibility);
    this->invalidate_materialized(index);
}

ItemNode ItemNodeList::take(const base::usize index)
{
    this->invalidate_materialized(index);
    return this->load_moved(index);
}

ItemNode ItemNodeList::operator[](const base::usize index) const
{
    return this->load(index);
}

const ItemNode* ItemNodeList::ptr(const base::usize index) const
{
    if (index >= this->headers_.size()) {
        return nullptr;
    }
    return &this->materialized(index);
}

base::u8 ItemNodeList::pack_kind(const ItemKind kind) noexcept
{
    return static_cast<base::u8>(kind);
}

base::u8 ItemNodeList::pack_visibility(const Visibility visibility) noexcept
{
    return static_cast<base::u8>(visibility);
}

bool ItemNodeList::has_flag(const base::u8 flags, const base::u8 flag) noexcept
{
    return (flags & flag) != 0;
}

base::u8 ItemNodeList::pack_flags(const ItemNode& node) noexcept
{
    base::u8 flags = 0;
    if (node.is_export_c) {
        flags |= ITEM_NODE_FLAG_EXPORT_C;
    }
    if (node.is_extern_c) {
        flags |= ITEM_NODE_FLAG_EXTERN_C;
    }
    if (node.is_unsafe) {
        flags |= ITEM_NODE_FLAG_UNSAFE;
    }
    if (node.is_variadic) {
        flags |= ITEM_NODE_FLAG_VARIADIC;
    }
    if (node.is_prototype) {
        flags |= ITEM_NODE_FLAG_PROTOTYPE;
    }
    if (node.is_trait_default_method) {
        flags |= ITEM_NODE_FLAG_TRAIT_DEFAULT_METHOD;
    }
    return flags;
}

GenericConstraintDecl ItemNodeList::copy_generic_constraint(const GenericConstraintDecl& constraint)
{
    GenericConstraintDecl copy;
    copy.param_name = constraint.param_name;
    copy.param_range = constraint.param_range;
    copy.capability_names = this->copy_list(constraint.capability_names);
    copy.capability_ranges = this->copy_list(constraint.capability_ranges);
    copy.capability_associated_constraints = constraint.capability_associated_constraints;
    copy.range = constraint.range;
    copy.param_name_id = constraint.param_name_id;
    copy.capability_name_ids = this->copy_list(constraint.capability_name_ids);
    return copy;
}

GenericConstraintDecl ItemNodeList::copy_or_move_generic_constraint(GenericConstraintDecl&& constraint)
{
    GenericConstraintDecl copy;
    copy.param_name = constraint.param_name;
    copy.param_range = constraint.param_range;
    copy.capability_names = this->copy_or_move_list(std::move(constraint.capability_names));
    copy.capability_ranges = this->copy_or_move_list(std::move(constraint.capability_ranges));
    copy.capability_associated_constraints = std::move(constraint.capability_associated_constraints);
    copy.range = constraint.range;
    copy.param_name_id = constraint.param_name_id;
    copy.capability_name_ids = this->copy_or_move_list(std::move(constraint.capability_name_ids));
    return copy;
}

EnumCaseDecl ItemNodeList::copy_enum_case(const EnumCaseDecl& enum_case)
{
    EnumCaseDecl copy;
    copy.name = enum_case.name;
    copy.payload_type = enum_case.payload_type;
    copy.payload_types = this->copy_list(enum_case.payload_types);
    copy.value_text = enum_case.value_text;
    copy.range = enum_case.range;
    copy.name_id = enum_case.name_id;
    return copy;
}

EnumCaseDecl ItemNodeList::copy_or_move_enum_case(EnumCaseDecl&& enum_case)
{
    EnumCaseDecl copy;
    copy.name = enum_case.name;
    copy.payload_type = enum_case.payload_type;
    copy.payload_types = this->copy_or_move_list(std::move(enum_case.payload_types));
    copy.value_text = enum_case.value_text;
    copy.range = enum_case.range;
    copy.name_id = enum_case.name_id;
    return copy;
}

GenericConstraintDecl ItemNodeList::detach_generic_constraint(const GenericConstraintDecl& constraint) const
{
    GenericConstraintDecl copy;
    copy.param_name = constraint.param_name;
    copy.param_range = constraint.param_range;
    copy.capability_names = copy_detached_ast_vector(constraint.capability_names);
    copy.capability_ranges = copy_detached_ast_vector(constraint.capability_ranges);
    copy.capability_associated_constraints = constraint.capability_associated_constraints;
    copy.range = constraint.range;
    copy.param_name_id = constraint.param_name_id;
    copy.capability_name_ids = copy_detached_ast_vector(constraint.capability_name_ids);
    return copy;
}

EnumCaseDecl ItemNodeList::detach_enum_case(const EnumCaseDecl& enum_case) const
{
    EnumCaseDecl copy;
    copy.name = enum_case.name;
    copy.payload_type = enum_case.payload_type;
    copy.payload_types = copy_detached_ast_vector(enum_case.payload_types);
    copy.value_text = enum_case.value_text;
    copy.range = enum_case.range;
    copy.name_id = enum_case.name_id;
    return copy;
}

base::u32 ItemNodeList::store_payload(ItemNode node)
{
    switch (node.kind) {
        case ItemKind::const_decl:
            return this->push_payload(this->payloads_.consts,
                ConstItemPayload{
                    node.name,
                    node.name_id,
                    node.const_type,
                    node.const_value,
                });
        case ItemKind::type_alias:
            return this->push_payload(this->payloads_.type_aliases,
                TypeAliasItemPayload{
                    node.name,
                    node.name_id,
                    this->copy_list(node.generic_params),
                    this->copy_or_move_generic_constraints(std::move(node.where_constraints)),
                    node.alias_type,
                    node.impl_type,
                    node.trait_type,
                });
        case ItemKind::struct_decl:
            return this->push_payload(this->payloads_.structs,
                StructItemPayload{
                    node.name,
                    node.name_id,
                    this->copy_list(node.generic_params),
                    this->copy_or_move_generic_constraints(std::move(node.where_constraints)),
                    this->copy_list(node.fields),
                });
        case ItemKind::enum_decl:
            return this->push_payload(this->payloads_.enums,
                EnumItemPayload{
                    node.name,
                    node.name_id,
                    this->copy_list(node.generic_params),
                    this->copy_or_move_generic_constraints(std::move(node.where_constraints)),
                    node.enum_base_type,
                    this->copy_or_move_enum_cases(std::move(node.enum_cases)),
                });
        case ItemKind::opaque_struct_decl:
            return this->push_payload(this->payloads_.opaque_structs,
                OpaqueStructItemPayload{
                    node.name,
                    node.name_id,
                });
        case ItemKind::trait_decl:
            return this->push_payload(this->payloads_.traits,
                TraitItemPayload{
                    node.name,
                    node.name_id,
                    this->copy_list(node.generic_params),
                    this->copy_or_move_generic_constraints(std::move(node.where_constraints)),
                    this->copy_list(node.trait_items),
                });
        case ItemKind::fn_decl:
            return this->push_payload(this->payloads_.functions,
                FunctionItemPayload{
                    node.name,
                    node.name_id,
                    this->copy_list(node.generic_params),
                    this->copy_or_move_generic_constraints(std::move(node.where_constraints)),
                    this->copy_list(node.params),
                    node.return_type,
                    node.body,
                    node.impl_type,
                    node.trait_type,
                    node.abi_name,
                });
        case ItemKind::extern_block:
            return this->push_payload(this->payloads_.extern_blocks,
                ExternBlockItemPayload{
                    this->copy_list(node.extern_items),
                });
        case ItemKind::impl_block:
            return this->push_payload(this->payloads_.impl_blocks,
                ImplBlockItemPayload{
                    this->copy_list(node.generic_params),
                    this->copy_or_move_generic_constraints(std::move(node.where_constraints)),
                    node.impl_type,
                    node.trait_type,
                    this->copy_list(node.impl_items),
                });
    }
    return this->push_payload(this->payloads_.unknowns, std::move(node));
}

void ItemNodeList::load_header(const ItemNodeHeader& header, ItemNode& node) const noexcept
{
    node.kind = static_cast<ItemKind>(header.kind);
    node.range = header.range;
    node.visibility = static_cast<Visibility>(header.visibility);
    node.is_export_c = has_flag(header.flags, ITEM_NODE_FLAG_EXPORT_C);
    node.is_extern_c = has_flag(header.flags, ITEM_NODE_FLAG_EXTERN_C);
    node.is_unsafe = has_flag(header.flags, ITEM_NODE_FLAG_UNSAFE);
    node.is_variadic = has_flag(header.flags, ITEM_NODE_FLAG_VARIADIC);
    node.is_prototype = has_flag(header.flags, ITEM_NODE_FLAG_PROTOTYPE);
    node.is_trait_default_method = has_flag(header.flags, ITEM_NODE_FLAG_TRAIT_DEFAULT_METHOD);
}

ItemNode ItemNodeList::load(const base::usize index) const
{
    const ItemNodeHeader& header = this->headers_[index];
    ItemNode node;
    this->load_header(header, node);
    switch (node.kind) {
        case ItemKind::const_decl: {
            const ConstItemPayload& payload = this->payloads_.consts[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.const_type = payload.type;
            node.const_value = payload.value;
            break;
        }
        case ItemKind::type_alias: {
            const TypeAliasItemPayload& payload = this->payloads_.type_aliases[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.alias_type = payload.target;
            node.impl_type = payload.impl_type;
            node.trait_type = payload.trait_type;
            break;
        }
        case ItemKind::struct_decl: {
            const StructItemPayload& payload = this->payloads_.structs[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.fields = copy_std_vector(payload.fields);
            break;
        }
        case ItemKind::enum_decl: {
            const EnumItemPayload& payload = this->payloads_.enums[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.enum_base_type = payload.base_type;
            node.enum_cases = this->detach_enum_cases(payload.cases);
            break;
        }
        case ItemKind::opaque_struct_decl:
            node.name = this->payloads_.opaque_structs[header.payload].name;
            node.name_id = this->payloads_.opaque_structs[header.payload].name_id;
            break;
        case ItemKind::trait_decl: {
            const TraitItemPayload& payload = this->payloads_.traits[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.trait_items = copy_std_vector(payload.items);
            break;
        }
        case ItemKind::fn_decl: {
            const FunctionItemPayload& payload = this->payloads_.functions[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.params = copy_std_vector(payload.params);
            node.return_type = payload.return_type;
            node.body = payload.body;
            node.impl_type = payload.impl_type;
            node.trait_type = payload.trait_type;
            node.abi_name = payload.abi_name;
            break;
        }
        case ItemKind::extern_block:
            node.extern_items = copy_std_vector(this->payloads_.extern_blocks[header.payload].items);
            break;
        case ItemKind::impl_block: {
            const ImplBlockItemPayload& payload = this->payloads_.impl_blocks[header.payload];
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.impl_type = payload.impl_type;
            node.trait_type = payload.trait_type;
            node.impl_items = copy_std_vector(payload.items);
            break;
        }
        default:
            node = this->payloads_.unknowns[header.payload];
            this->load_header(header, node);
            break;
    }
    return node;
}

ItemNode ItemNodeList::load_moved(const base::usize index)
{
    const ItemNodeHeader& header = this->headers_[index];
    ItemNode node;
    this->load_header(header, node);
    switch (node.kind) {
        case ItemKind::const_decl: {
            const ConstItemPayload& payload = this->payloads_.consts[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.const_type = payload.type;
            node.const_value = payload.value;
            break;
        }
        case ItemKind::type_alias: {
            TypeAliasItemPayload& payload = this->payloads_.type_aliases[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.alias_type = payload.target;
            node.impl_type = payload.impl_type;
            node.trait_type = payload.trait_type;
            break;
        }
        case ItemKind::struct_decl: {
            StructItemPayload& payload = this->payloads_.structs[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.fields = copy_std_vector(payload.fields);
            break;
        }
        case ItemKind::enum_decl: {
            EnumItemPayload& payload = this->payloads_.enums[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.enum_base_type = payload.base_type;
            node.enum_cases = this->detach_enum_cases(payload.cases);
            break;
        }
        case ItemKind::opaque_struct_decl:
            node.name = this->payloads_.opaque_structs[header.payload].name;
            node.name_id = this->payloads_.opaque_structs[header.payload].name_id;
            break;
        case ItemKind::trait_decl: {
            TraitItemPayload& payload = this->payloads_.traits[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.trait_items = copy_std_vector(payload.items);
            break;
        }
        case ItemKind::fn_decl: {
            FunctionItemPayload& payload = this->payloads_.functions[header.payload];
            node.name = payload.name;
            node.name_id = payload.name_id;
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.params = copy_std_vector(payload.params);
            node.return_type = payload.return_type;
            node.body = payload.body;
            node.impl_type = payload.impl_type;
            node.trait_type = payload.trait_type;
            node.abi_name = payload.abi_name;
            break;
        }
        case ItemKind::extern_block:
            node.extern_items = copy_std_vector(this->payloads_.extern_blocks[header.payload].items);
            break;
        case ItemKind::impl_block: {
            ImplBlockItemPayload& payload = this->payloads_.impl_blocks[header.payload];
            node.generic_params = copy_std_vector(payload.generic_params);
            node.where_constraints = this->detach_generic_constraints(payload.where_constraints);
            node.impl_type = payload.impl_type;
            node.trait_type = payload.trait_type;
            node.impl_items = copy_std_vector(payload.items);
            break;
        }
        default:
            node = std::move(this->payloads_.unknowns[header.payload]);
            this->load_header(header, node);
            break;
    }
    return node;
}

const ItemNode& ItemNodeList::materialized(const base::usize index) const
{
    this->ensure_materialized_capacity(index + 1);
    if (!this->materialized_valid_[index]) {
        this->materialized_[index] = this->load(index);
        this->materialized_valid_[index] = true;
    }
    return this->materialized_[index];
}

void ItemNodeList::ensure_materialized_capacity(const base::usize size) const
{
    if (this->materialized_.size() < size) {
        this->materialized_.resize(size);
    }
    if (this->materialized_valid_.size() < size) {
        this->materialized_valid_.resize(size, false);
    }
}

void ItemNodeList::invalidate_materialized(const base::usize index) const
{
    if (index < this->materialized_valid_.size()) {
        this->materialized_valid_[index] = false;
    }
}

void ItemNodeList::copy_from(const ItemNodeList& other)
{
    this->reserve(other.size());
    for (base::usize i = 0; i < other.size(); ++i) {
        static_cast<void>(this->append(other.load(i)));
    }
}

void ItemNodeList::swap(ItemNodeList& other) noexcept
{
    using std::swap;
    swap(this->arena_, other.arena_);
    this->headers_.swap(other.headers_);
    this->payloads_.swap(other.payloads_);
    this->materialized_.swap(other.materialized_);
    this->materialized_valid_.swap(other.materialized_valid_);
}

} // namespace aurex::syntax
