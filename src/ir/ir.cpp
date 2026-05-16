#include <aurex/ir/ir.hpp>

#include <utility>

namespace aurex::ir {

RecordLayout::RecordLayout() = default;

RecordLayout::RecordLayout(base::BumpAllocator& arena)
    : fields(base::BumpAllocatorAdapter<RecordField> {arena}) {}

Value::Value() = default;

Value::Value(base::BumpAllocator& arena)
    : args(base::BumpAllocatorAdapter<ValueId> {arena}),
      fields(base::BumpAllocatorAdapter<FieldValue> {arena}),
      incoming(base::BumpAllocatorAdapter<PhiInput> {arena}),
      elements(base::BumpAllocatorAdapter<ValueId> {arena}) {}

BasicBlock::BasicBlock() = default;

BasicBlock::BasicBlock(base::BumpAllocator& arena)
    : values(base::BumpAllocatorAdapter<ValueId> {arena}) {}

Function::Function() = default;

Function::Function(base::BumpAllocator& arena)
    : signature_params(base::BumpAllocatorAdapter<FunctionParam> {arena}),
      param_values(base::BumpAllocatorAdapter<ValueId> {arena}),
      blocks(base::BumpAllocatorAdapter<BasicBlock> {arena}) {}

Module::Module()
    : arena_(std::make_unique<base::BumpAllocator>()),
      constants(this->make_vector<GlobalConstant>()),
      records(this->make_vector<RecordLayout>()),
      values(this->make_vector<Value>()),
      functions(this->make_vector<Function>()),
      record_indices(this->make_map<base::u32, base::u32>()) {}

Module::Module(const Module& other)
    : Module() {
    this->copy_from(other);
}

Module& Module::operator=(const Module& other) {
    if (this == &other) {
        return *this;
    }
    Module copy(other);
    *this = std::move(copy);
    return *this;
}

Module::Module(Module&& other) noexcept
    : arena_(std::move(other.arena_)),
      types(std::move(other.types)),
      identifiers(std::move(other.identifiers)),
      constants(std::move(other.constants)),
      records(std::move(other.records)),
      values(std::move(other.values)),
      functions(std::move(other.functions)),
      record_indices(std::move(other.record_indices)) {
    other.ensure_arena();
}

Module& Module::operator=(Module&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

IrTextId Module::intern(const std::string_view text) {
    return this->identifiers.intern(text);
}

IrTextId Module::find_text(const std::string_view text) const noexcept {
    return this->identifiers.find(text);
}

std::string_view Module::text(const IrTextId id) const noexcept {
    return this->identifiers.text(id);
}

bool Module::has_text(const IrTextId id) const noexcept {
    return sema::is_valid(id) && id.value < this->identifiers.size();
}

Value Module::make_value() {
    this->ensure_arena();
    return Value {*this->arena_};
}

Function Module::make_function() {
    this->ensure_arena();
    return Function {*this->arena_};
}

BasicBlock Module::make_block() {
    this->ensure_arena();
    return BasicBlock {*this->arena_};
}

RecordLayout Module::make_record_layout() {
    this->ensure_arena();
    return RecordLayout {*this->arena_};
}

Value Module::clone_value(const Value& other) {
    Value copy = this->make_value();
    copy.kind = other.kind;
    copy.type = other.type;
    copy.name = other.name;
    copy.text = other.text;
    copy.call_target = other.call_target;
    copy.lhs = other.lhs;
    copy.rhs = other.rhs;
    copy.object = other.object;
    copy.index = other.index;
    copy.args = this->copy_vector<ValueId>({other.args.data(), other.args.size()});
    copy.fields = this->copy_vector<FieldValue>({other.fields.data(), other.fields.size()});
    copy.incoming = this->copy_vector<PhiInput>({other.incoming.data(), other.incoming.size()});
    copy.constant = other.constant;
    copy.elements = this->copy_vector<ValueId>({other.elements.data(), other.elements.size()});
    copy.unary_op = other.unary_op;
    copy.binary_op = other.binary_op;
    copy.cast_kind = other.cast_kind;
    copy.target_type = other.target_type;
    return copy;
}

Function Module::clone_function(const Function& other) {
    Function copy = this->make_function();
    copy.name = other.name;
    copy.symbol = other.symbol;
    copy.linkage = other.linkage;
    copy.call_conv = other.call_conv;
    copy.is_entry = other.is_entry;
    copy.is_unsafe = other.is_unsafe;
    copy.is_variadic = other.is_variadic;
    copy.return_type = other.return_type;
    copy.signature_params = this->copy_vector<FunctionParam>({other.signature_params.data(), other.signature_params.size()});
    copy.param_values = this->copy_vector<ValueId>({other.param_values.data(), other.param_values.size()});
    copy.blocks.reserve(other.blocks.size());
    for (const BasicBlock& block : other.blocks) {
        copy.blocks.push_back(this->clone_block(block));
    }
    return copy;
}

BasicBlock Module::clone_block(const BasicBlock& other) {
    BasicBlock copy = this->make_block();
    copy.name = other.name;
    copy.values = this->copy_vector<ValueId>({other.values.data(), other.values.size()});
    copy.terminator = other.terminator;
    return copy;
}

RecordLayout Module::clone_record_layout(const RecordLayout& other) {
    RecordLayout copy = this->make_record_layout();
    copy.type = other.type;
    copy.name = other.name;
    copy.symbol = other.symbol;
    copy.is_opaque = other.is_opaque;
    copy.fields = this->copy_vector<RecordField>({other.fields.data(), other.fields.size()});
    return copy;
}

void Module::reserve(
    const base::usize value_count,
    const base::usize function_count,
    const base::usize record_count,
    const base::usize constant_count
) {
    this->values.reserve(value_count);
    this->functions.reserve(function_count);
    this->records.reserve(record_count);
    this->constants.reserve(constant_count);
    this->record_indices.reserve(record_count);
    this->identifiers.reserve(value_count + function_count + record_count + constant_count);
}

void Module::swap(Module& other) noexcept {
    using std::swap;
    swap(this->types, other.types);
    swap(this->identifiers, other.identifiers);
    this->constants.swap(other.constants);
    this->records.swap(other.records);
    this->values.swap(other.values);
    this->functions.swap(other.functions);
    this->record_indices.swap(other.record_indices);
    swap(this->arena_, other.arena_);
}

void Module::copy_from(const Module& other) {
    this->types = other.types;
    this->identifiers = other.identifiers;

    this->constants.clear();
    this->constants.reserve(other.constants.size());
    for (const GlobalConstant& constant : other.constants) {
        this->constants.push_back(constant);
    }

    this->records.clear();
    this->records.reserve(other.records.size());
    for (const RecordLayout& record : other.records) {
        this->records.push_back(this->clone_record_layout(record));
    }

    this->values.clear();
    this->values.reserve(other.values.size());
    for (const Value& value : other.values) {
        this->values.push_back(this->clone_value(value));
    }

    this->functions.clear();
    this->functions.reserve(other.functions.size());
    for (const Function& function : other.functions) {
        this->functions.push_back(this->clone_function(function));
    }

    this->record_indices.clear();
    this->record_indices.reserve(other.record_indices.size());
    for (const auto& entry : other.record_indices) {
        this->record_indices.emplace(entry.first, entry.second);
    }
}

void Module::ensure_arena() {
    if (this->arena_ != nullptr) {
        return;
    }
    this->arena_ = std::make_unique<base::BumpAllocator>();
    this->constants = this->make_vector<GlobalConstant>();
    this->records = this->make_vector<RecordLayout>();
    this->values = this->make_vector<Value>();
    this->functions = this->make_vector<Function>();
    this->record_indices = this->make_map<base::u32, base::u32>();
}

ValueId add_value(Module& module, Value value) {
    const ValueId id {static_cast<base::u32>(module.values.size())};
    module.values.push_back(module.clone_value(value));
    return id;
}

GlobalConstantId add_global_constant(Module& module, GlobalConstant constant) {
    const GlobalConstantId id {static_cast<base::u32>(module.constants.size())};
    module.constants.push_back(constant);
    return id;
}

FunctionId add_function(Module& module, Function function) {
    const FunctionId id {static_cast<base::u32>(module.functions.size())};
    module.functions.push_back(module.clone_function(function));
    return id;
}

base::u32 add_record(Module& module, RecordLayout record) {
    const base::u32 index = static_cast<base::u32>(module.records.size());
    module.records.push_back(module.clone_record_layout(record));
    return index;
}

BlockId add_block(Module& module, Function& function, const std::string_view name) {
    const BlockId id {static_cast<base::u32>(function.blocks.size())};
    BasicBlock block = module.make_block();
    block.name = module.intern(name);
    function.blocks.push_back(std::move(block));
    return id;
}

const GlobalConstant* find_global_constant(const Module& module, const GlobalConstantId id) noexcept {
    if (!is_valid(id) || id.value >= module.constants.size()) {
        return nullptr;
    }
    return &module.constants[id.value];
}

const RecordLayout* find_record(const Module& module, const sema::TypeHandle type) noexcept {
    if (!sema::is_valid(type)) {
        return nullptr;
    }
    if (const auto found = module.record_indices.find(type.value);
        found != module.record_indices.end() &&
        found->second < module.records.size() &&
        module.types.same(module.records[found->second].type, type)) {
        return &module.records[found->second];
    }
    for (const RecordLayout& record : module.records) {
        if (module.types.same(record.type, type)) {
            return &record;
        }
    }
    return nullptr;
}

const RecordField* find_record_field(
    Module& module,
    const sema::TypeHandle type,
    const std::string_view name
) noexcept {
    return find_record_field(module, type, module.intern(name));
}

const RecordField* find_record_field(const Module& module, const sema::TypeHandle type, const IrTextId name) noexcept {
    const RecordLayout* record = find_record(module, type);
    if (record == nullptr) {
        return nullptr;
    }
    for (const RecordField& field : record->fields) {
        if (field.name == name) {
            return &field;
        }
    }
    return nullptr;
}

base::usize record_field_index(const RecordLayout& record, const IrTextId name) noexcept {
    for (base::usize i = 0; i < record.fields.size(); ++i) {
        if (record.fields[i].name == name) {
            return i;
        }
    }
    return static_cast<base::usize>(-1);
}

} // namespace aurex::ir
