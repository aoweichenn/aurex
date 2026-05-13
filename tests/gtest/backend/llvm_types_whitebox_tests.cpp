#include <aurex/backend/llvm_backend.hpp>
#include <aurex/ir/ir.hpp>
#include <aurex/sema/type.hpp>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Target/TargetMachine.h>

#include <memory>
#include <string>
#include <unordered_map>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include <backend/llvm/llvm_backend_internal.hpp>
#undef private
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <gtest/gtest.h>
#include <gtest/support/ir_test_helpers.hpp>

namespace aurex::test {
namespace {

using namespace irtest;

} // namespace

TEST(CoreUnit, LlvmBackendWhiteBoxCoversFunctionTypeHelperEdges) {
    Module module;
    const TypeHandle void_type = builtin(module, BuiltinType::void_);
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle u32 = builtin(module, BuiltinType::u32);
    const TypeHandle char_type = builtin(module, BuiltinType::char_);
    const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
    const TypeHandle generic_param = module.types.generic_param("T");
    const TypeHandle enum_without_underlying = module.types.named_enum("unit.MissingTag", "unit_MissingTag");
    const TypeHandle function_type = module.types.function(
        sema::FunctionCallConv::aurex,
        false,
        std::vector<TypeHandle> {i32, u32},
        i32
    );
    const TypeHandle extern_variadic_type = module.types.function(
        sema::FunctionCallConv::c,
        true,
        std::vector<TypeHandle> {},
        void_type
    );
    const TypeHandle opaque_record = module.types.opaque_struct("unit.Opaque", "unit_Opaque");
    module.records.push_back(RecordLayout {
        sema::INVALID_TYPE_HANDLE,
        "unit.Invalid",
        "unit_Invalid",
        false,
        {},
    });
    module.records.push_back(RecordLayout {
        opaque_record,
        "unit.Opaque",
        "",
        true,
        {},
    });

    Value plain_value;
    plain_value.kind = ValueKind::param;
    plain_value.type = i32;
    const ValueId plain_value_id = add_value(module, plain_value);
    Value pointer_value;
    pointer_value.kind = ValueKind::param;
    pointer_value.type = ptr_i32;
    const ValueId pointer_value_id = add_value(module, pointer_value);

    backend::LlvmEmitter emitter(module, "unit_backend_types_whitebox");
    auto target = emitter.configure_target();
    ASSERT_TRUE(target) << target.error().message;

    emitter.declare_records();
    EXPECT_TRUE(emitter.llvm_type(sema::INVALID_TYPE_HANDLE)->isVoidTy());
    EXPECT_TRUE(emitter.llvm_type(generic_param)->isVoidTy());
    EXPECT_TRUE(emitter.llvm_type(enum_without_underlying)->isVoidTy());
    EXPECT_TRUE(emitter.llvm_type(function_type)->isPointerTy());
    EXPECT_TRUE(emitter.llvm_type(extern_variadic_type)->isPointerTy());
    EXPECT_TRUE(emitter.llvm_type(char_type)->isIntegerTy(32));

    llvm::FunctionType* invalid_function = emitter.llvm_function_type(i32);
    ASSERT_NE(invalid_function, nullptr);
    EXPECT_TRUE(invalid_function->getReturnType()->isVoidTy());
    EXPECT_EQ(invalid_function->getNumParams(), 0U);
    EXPECT_FALSE(invalid_function->isVarArg());

    llvm::FunctionType* aurex_function = emitter.llvm_function_type(function_type);
    ASSERT_NE(aurex_function, nullptr);
    EXPECT_TRUE(aurex_function->getReturnType()->isIntegerTy(32));
    EXPECT_EQ(aurex_function->getNumParams(), 2U);
    EXPECT_FALSE(aurex_function->isVarArg());

    llvm::FunctionType* c_variadic_function = emitter.llvm_function_type(extern_variadic_type);
    ASSERT_NE(c_variadic_function, nullptr);
    EXPECT_TRUE(c_variadic_function->getReturnType()->isVoidTy());
    EXPECT_EQ(c_variadic_function->getNumParams(), 0U);
    EXPECT_TRUE(c_variadic_function->isVarArg());

    EXPECT_TRUE(emitter.pointee_llvm_type(i32)->isVoidTy());
    EXPECT_TRUE(emitter.pointee_llvm_type(ptr_i32)->isIntegerTy(32));
    EXPECT_EQ(emitter.pointee_type(plain_value_id).value, sema::INVALID_TYPE_HANDLE.value);
    EXPECT_EQ(emitter.pointee_type(pointer_value_id).value, i32.value);
    EXPECT_FALSE(emitter.is_unsigned_integer(sema::INVALID_TYPE_HANDLE));
    EXPECT_FALSE(emitter.is_unsigned_integer(enum_without_underlying));
    EXPECT_FALSE(emitter.is_unsigned_integer(function_type));
    EXPECT_FALSE(emitter.is_unsigned_integer(char_type));
    EXPECT_TRUE(emitter.is_unsigned_integer(u32));
}

} // namespace aurex::test
