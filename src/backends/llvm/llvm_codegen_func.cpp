//
// Created by Mike Smith on 2022/6/6.
//

#include <backends/llvm/llvm_codegen.h>

namespace luisa::compute::llvm {

luisa::string LLVMCodegen::_function_name(Function f) const noexcept {
    return luisa::format("{}.{:016x}",
                         f.tag() == Function::Tag::KERNEL ? "kernel" : "custom",
                         f.hash());
}

unique_ptr<LLVMCodegen::FunctionContext> LLVMCodegen::_create_kernel_context(Function f) noexcept {
    auto create_argument_struct = [&] {
        auto member_index = 0u;
        luisa::vector<::llvm::Type *> field_types;
        luisa::vector<uint> field_indices;
        auto size = 0ul;
        static constexpr auto alignment = 16ul;
        for (auto &arg : f.arguments()) {
            auto aligned_offset = luisa::align(size, alignment);
            if (aligned_offset > size) {
                auto padding = ::llvm::ArrayType::get(
                    ::llvm::Type::getInt8Ty(_context), aligned_offset - size);
                field_types.emplace_back(padding);
                member_index++;
            }
            auto arg_size = [arg]() noexcept -> size_t {
                switch (arg.tag()) {
                    case Variable::Tag::REFERENCE: return sizeof(void *);
                    case Variable::Tag::BUFFER: return buffer_handle_size;
                    case Variable::Tag::TEXTURE: return texture_handle_size;
                    case Variable::Tag::BINDLESS_ARRAY: return bindless_array_handle_size;
                    case Variable::Tag::ACCEL: return accel_handle_size;
                    default: break;
                }
                return arg.type()->size();
            }();
            field_types.emplace_back(_create_type(arg.type()));
            field_indices.emplace_back(member_index++);
            size = aligned_offset + arg_size;
        }
        auto aligned_size = luisa::align(size, alignment);
        if (aligned_size > size) {// last padding
            auto padding = ::llvm::ArrayType::get(
                ::llvm::Type::getInt8Ty(_context), aligned_size - size);
            field_types.emplace_back(padding);
            member_index++;
        }
        // launch size
        field_types.emplace_back(_create_type(Type::of<uint3>()));
        field_indices.emplace_back(member_index++);
        ::llvm::ArrayRef<::llvm::Type *> fields_ref{field_types.data(), field_types.size()};
        return std::make_pair(
            ::llvm::StructType::create(_context, fields_ref),
            std::move(field_indices));
    };
    auto [arg_struct_type, arg_struct_indices] = create_argument_struct();
    auto arg_struct_name = luisa::format("arg.buffer.struct.{:016x}", f.hash());
    arg_struct_type->setName(::llvm::StringRef{arg_struct_name.data(), arg_struct_name.size()});
    auto arg_buffer_type = ::llvm::PointerType::get(arg_struct_type, 0);
    ::llvm::SmallVector<::llvm::Type *, 8u> arg_types;
    arg_types.emplace_back(arg_buffer_type);
    arg_types.emplace_back(::llvm::PointerType::get(::llvm::Type::getInt8Ty(_context), 0));
    auto i32_type = ::llvm::Type::getInt32Ty(_context);
    // dispatch size
    arg_types.emplace_back(i32_type);
    arg_types.emplace_back(i32_type);
    arg_types.emplace_back(i32_type);
    // block id
    arg_types.emplace_back(i32_type);
    arg_types.emplace_back(i32_type);
    arg_types.emplace_back(i32_type);
    LUISA_ASSERT(f.return_type() == nullptr,
                 "Invalid return type '{}' for kernel. Only void is allowed.",
                 f.return_type()->description());
    auto function_type = ::llvm::FunctionType::get(
        ::llvm::Type::getVoidTy(_context), arg_types, false);
    auto main_name = luisa::format("kernel.{:016x}.main", f.hash());
    auto ir = ::llvm::Function::Create(
        function_type, ::llvm::Function::ExternalLinkage,
        ::llvm::StringRef{main_name.data(), main_name.size()}, _module);
    auto builder = luisa::make_unique<::llvm::IRBuilder<>>(_context);
    auto body_block = ::llvm::BasicBlock::Create(_context, "entry", ir);
    builder->SetInsertPoint(body_block);
    luisa::vector<::llvm::Value *> arguments;
    for (auto arg_id = 0u; arg_id < f.arguments().size(); arg_id++) {
        auto &arg = f.arguments()[arg_id];
        switch (arg.tag()) {
            case Variable::Tag::LOCAL:
            case Variable::Tag::BUFFER:
            case Variable::Tag::TEXTURE:
            case Variable::Tag::BINDLESS_ARRAY:
            case Variable::Tag::ACCEL: {
                auto arg_name = _variable_name(arg);
                auto addr_name = luisa::format("{}.addr", arg_name);
                auto pr = builder->CreateStructGEP(
                    arg_struct_type, ir->getArg(0u), arg_struct_indices[arg_id],
                    ::llvm::StringRef{addr_name.data(), addr_name.size()});
                auto r = builder->CreateAlignedLoad(
                    _create_type(arg.type()), pr, ::llvm::Align{16ull},
                    ::llvm::StringRef{arg_name.data(), arg_name.size()});
                arguments.emplace_back(r);
                break;
            }
            default: LUISA_ERROR_WITH_LOCATION("Invalid kernel argument type.");
        }
    }
    auto exit_block = ::llvm::BasicBlock::Create(_context, "exit", ir);
    builder->SetInsertPoint(exit_block);
    builder->CreateRetVoid();

    // create work function...
    auto ctx = _create_kernel_program(f);
    // create simple schedule...
    builder->SetInsertPoint(body_block);
    // block id
    auto block_id = static_cast<::llvm::Value *>(
        ::llvm::UndefValue::get(::llvm::FixedVectorType::get(i32_type, 3u)));
    auto dispatch_size = static_cast<::llvm::Value *>(
        ::llvm::UndefValue::get(::llvm::FixedVectorType::get(i32_type, 3u)));
    for (auto i = 0u; i < 3u; i++) {
        using namespace std::string_literals;
        std::array elements{"x", "y", "z"};
        auto block_id_i = ir->getArg(i + 5u);
        auto dispatch_size_i = ir->getArg(i + 2u);
        block_id_i->setName("block.id."s.append(elements[i]));
        dispatch_size_i->setName("dispatch.size."s.append(elements[i]));
        block_id = builder->CreateInsertElement(block_id, block_id_i, i);
        dispatch_size = builder->CreateInsertElement(dispatch_size, dispatch_size_i, i);
    }
    // loop
    auto p_index = builder->CreateAlloca(::llvm::Type::getInt32Ty(_context), nullptr, "loop.index.addr");
    p_index->setAlignment(::llvm::Align{16});
    builder->CreateStore(builder->getInt32(0u), p_index);
    auto loop_block = ::llvm::BasicBlock::Create(_context, "loop.head", ir, exit_block);
    builder->CreateBr(loop_block);
    builder->SetInsertPoint(loop_block);
    auto index = builder->CreateLoad(::llvm::Type::getInt32Ty(_context), p_index, "loop.index");
    auto thread_yz = builder->CreateUDiv(index, builder->getInt32(f.block_size().x));
    auto thread_x = builder->CreateURem(index, builder->getInt32(f.block_size().x), "thread.id.x");
    auto thread_y = builder->CreateURem(thread_yz, builder->getInt32(f.block_size().y), "thread.id.y");
    auto thread_z = builder->CreateUDiv(thread_yz, builder->getInt32(f.block_size().y), "thread.id.z");
    auto thread_id = static_cast<::llvm::Value *>(::llvm::UndefValue::get(_create_type(Type::of<uint3>())));
    thread_id = builder->CreateInsertElement(thread_id, thread_x, static_cast<uint64_t>(0u));
    thread_id = builder->CreateInsertElement(thread_id, thread_y, static_cast<uint64_t>(1u));
    thread_id = builder->CreateInsertElement(thread_id, thread_z, static_cast<uint64_t>(2u), "thread.id");
    auto dispatch_id = builder->CreateNUWAdd(builder->CreateNUWMul(block_id, _literal(f.block_size())), thread_id, "dispatch.id");
    auto valid_thread = builder->CreateAndReduce(builder->CreateICmpULT(dispatch_id, dispatch_size, "thread.id.cmp"));
    auto call_block = ::llvm::BasicBlock::Create(_context, "loop.kernel", ir, exit_block);
    auto loop_update_block = ::llvm::BasicBlock::Create(_context, "loop.update", ir, exit_block);
    builder->CreateCondBr(valid_thread, call_block, loop_update_block);
    builder->SetInsertPoint(call_block);
    auto call_args = arguments;
    call_args.emplace_back(thread_id);
    call_args.emplace_back(block_id);
    call_args.emplace_back(dispatch_id);
    call_args.emplace_back(dispatch_size);
    call_args.emplace_back(ir->getArg(1u));// shared memory
    builder->CreateCall(ctx->ir->getFunctionType(), ctx->ir,
                        ::llvm::ArrayRef<::llvm::Value *>{call_args.data(), call_args.size()});
    // update
    builder->CreateBr(loop_update_block);
    builder->SetInsertPoint(loop_update_block);
    auto next_index = builder->CreateAdd(index, builder->getInt32(1u), "loop.index.next");
    builder->CreateStore(next_index, p_index);
    auto thread_count = f.block_size().x * f.block_size().y * f.block_size().z;
    auto should_continue = builder->CreateICmpULT(next_index, builder->getInt32(thread_count));
    builder->CreateCondBr(should_continue, loop_block, exit_block);
    return ctx;
}

luisa::unique_ptr<LLVMCodegen::FunctionContext> LLVMCodegen::_create_kernel_program(Function f) noexcept {
    luisa::vector<::llvm::Type *> arg_types;
    for (auto &&arg : f.arguments()) {
        switch (arg.tag()) {
            case Variable::Tag::LOCAL:
            case Variable::Tag::BUFFER:
            case Variable::Tag::TEXTURE:
            case Variable::Tag::BINDLESS_ARRAY:
            case Variable::Tag::ACCEL:
                arg_types.emplace_back(_create_type(arg.type()));
                break;
            default: LUISA_ERROR_WITH_LOCATION("Invalid kernel argument type.");
        }
    }
    // thread_id/block_id/dispatch_id/dispatch_size
    for (auto i = 0u; i < 4u; i++) {
        arg_types.emplace_back(_create_type(Type::of<uint3>()));
    }
    // shared memory
    arg_types.emplace_back(::llvm::PointerType::get(::llvm::Type::getInt8Ty(_context), 0u));
    auto needs_coroutine = f.builtin_callables().test(CallOp::SYNCHRONIZE_BLOCK);
    auto return_type = needs_coroutine ? ::llvm::Type::getVoidTy(_context) : ::llvm::Type::getInt8PtrTy(_context);
    ::llvm::ArrayRef<::llvm::Type *> arg_types_ref{arg_types.data(), arg_types.size()};
    auto function_type = ::llvm::FunctionType::get(return_type, arg_types_ref, false);
    auto name = _function_name(f);
    auto ir = ::llvm::Function::Create(
        function_type, ::llvm::Function::InternalLinkage,
        ::llvm::StringRef{name.data(), name.size()}, _module);
    auto builder = luisa::make_unique<::llvm::IRBuilder<>>(_context);
    auto body_block = ::llvm::BasicBlock::Create(_context, "entry", ir);
    auto exit_block = ::llvm::BasicBlock::Create(_context, "exit", ir);
    builder->SetInsertPoint(exit_block);
    if (needs_coroutine) {
        builder->CreateRetVoid();
    } else {
        auto shared_memory = ir->getArg(5u);
        builder->CreateRet(::llvm::ConstantPointerNull::get(
            ::llvm::Type::getInt8PtrTy(_context)));
    }
    builder->SetInsertPoint(body_block);
    luisa::unordered_map<uint, ::llvm::Value *> variables;
    auto make_alloca = [&](::llvm::Value *x, luisa::string_view name = "") noexcept {
        auto p = builder->CreateAlloca(
            x->getType(), nullptr,
            ::llvm::StringRef{name.data(), name.size()});
        p->setAlignment(::llvm::Align{16});
        builder->CreateStore(x, p);
        return p;
    };
    for (auto i = 0u; i < f.arguments().size(); i++) {
        auto lc_arg = f.arguments()[i];
        auto arg_name = _variable_name(lc_arg);
        auto arg = static_cast<::llvm::Value *>(ir->getArg(i));
        if (lc_arg.tag() == Variable::Tag::LOCAL) { arg = make_alloca(arg); }
        arg->setName(::llvm::StringRef{arg_name.data(), arg_name.size()});
        variables.emplace(lc_arg.uid(), arg);
    }
    auto builtin_offset = f.arguments().size();
    for (auto arg : f.builtin_variables()) {
        switch (arg.tag()) {
            case Variable::Tag::THREAD_ID:
                variables.emplace(arg.uid(), make_alloca(ir->getArg(builtin_offset + 0), _variable_name(arg)));
                break;
            case Variable::Tag::BLOCK_ID:
                variables.emplace(arg.uid(), make_alloca(ir->getArg(builtin_offset + 1), _variable_name(arg)));
                break;
            case Variable::Tag::DISPATCH_ID:
                variables.emplace(arg.uid(), make_alloca(ir->getArg(builtin_offset + 2), _variable_name(arg)));
                break;
            case Variable::Tag::DISPATCH_SIZE:
                variables.emplace(arg.uid(), make_alloca(ir->getArg(builtin_offset + 3), _variable_name(arg)));
                break;
            default: LUISA_ERROR_WITH_LOCATION("Invalid kernel argument type.");
        }
    }
    // shared memory
    auto smem = ir->getArg(builtin_offset + 4);
    auto smem_offset = 0u;
    for (auto s : f.shared_variables()) {
        auto alignment = s.type()->alignment();
        smem_offset = (smem_offset + alignment - 1u) / alignment * alignment;
        auto p = builder->CreateConstGEP1_32(
            ::llvm::Type::getInt8Ty(_context), smem, smem_offset);
        auto ptr = builder->CreatePointerCast(p, _create_type(s.type())->getPointerTo());
        variables.emplace(s.uid(), ptr);
        smem_offset += s.type()->size();
    }
    return luisa::make_unique<FunctionContext>(
        f, ir, nullptr, exit_block, std::move(builder), std::move(variables));
}

unique_ptr<LLVMCodegen::FunctionContext> LLVMCodegen::_create_callable_context(Function f) noexcept {
    auto is_out_reference = [&f](auto v) noexcept {
        return v.tag() == Variable::Tag::REFERENCE &&
               (f.variable_usage(v.uid()) == Usage::WRITE ||
                f.variable_usage(v.uid()) == Usage::READ_WRITE);
    };
    luisa::vector<::llvm::Type *> arg_types;
    for (auto &&arg : f.arguments()) {
        auto arg_type = _create_type(arg.type());
        if (is_out_reference(arg)) {
            arg_type = ::llvm::PointerType::get(arg_type, 0);
        }
        arg_types.emplace_back(arg_type);
    }
    auto return_type = _create_type(f.return_type());
    ::llvm::ArrayRef<::llvm::Type *> arg_types_ref{arg_types.data(), arg_types.size()};
    auto function_type = ::llvm::FunctionType::get(return_type, arg_types_ref, false);
    auto name = _function_name(f);
    auto ir = ::llvm::Function::Create(
        function_type, ::llvm::Function::InternalLinkage,
        ::llvm::StringRef{name.data(), name.size()}, _module);
    auto builder = luisa::make_unique<::llvm::IRBuilder<>>(_context);
    auto body_block = ::llvm::BasicBlock::Create(_context, "entry", ir);
    auto exit_block = ::llvm::BasicBlock::Create(_context, "exit", ir);
    builder->SetInsertPoint(body_block);
    auto i = 0u;
    luisa::unordered_map<uint, ::llvm::Value *> variables;
    for (auto &&arg : ir->args()) {
        auto lc_arg = f.arguments()[i++];
        auto arg_name = _variable_name(lc_arg);
        arg.setName(::llvm::StringRef{arg_name.data(), arg_name.size()});
        if (is_out_reference(lc_arg)) {
            variables.emplace(lc_arg.uid(), &arg);
        } else {
            switch (lc_arg.tag()) {
                case Variable::Tag::BUFFER:
                case Variable::Tag::TEXTURE:
                case Variable::Tag::BINDLESS_ARRAY:
                case Variable::Tag::ACCEL:
                    variables.emplace(lc_arg.uid(), &arg);
                    break;
                default: {
                    auto p_arg = builder->CreateAlloca(arg.getType());
                    p_arg->setAlignment(::llvm::Align{16});
                    builder->CreateStore(&arg, p_arg);
                    variables.emplace(lc_arg.uid(), p_arg);
                    break;
                }
            }
        }
    }
    ::llvm::Value *ret = nullptr;
    if (auto ret_type = f.return_type()) {
        builder->SetInsertPoint(body_block);
        auto p_ret = builder->CreateAlloca(return_type, nullptr, "retval.addr");
        p_ret->setAlignment(::llvm::Align{16});
        ret = p_ret;
        builder->SetInsertPoint(exit_block);
        builder->CreateRet(builder->CreateLoad(return_type, ret));
    } else {// return void
        builder->SetInsertPoint(exit_block);
        builder->CreateRetVoid();
    }
    builder->SetInsertPoint(body_block);
    return luisa::make_unique<FunctionContext>(
        f, ir, ret, exit_block, std::move(builder), std::move(variables));
}

::llvm::Function *LLVMCodegen::_create_function(Function f) noexcept {
    auto name = _function_name(f);
    ::llvm::StringRef name_ref{name.data(), name.size()};
    if (auto ir = _module->getFunction(name_ref)) { return ir; }
    _function_stack.emplace_back(
        f.tag() == Function::Tag::KERNEL ?
            _create_kernel_context(f) :
            _create_callable_context(f));
    _emit_function();
    auto ctx = std::move(_function_stack.back());
    _function_stack.pop_back();
    if (ctx->builder->GetInsertBlock()->getTerminator() == nullptr) {
        ctx->builder->CreateBr(ctx->exit_block);
    }
    return ctx->ir;
}

void LLVMCodegen::_emit_function() noexcept {
    auto ctx = _current_context();
    for (auto v : ctx->function.local_variables()) {
        auto p = _create_alloca(_create_type(v.type()), _variable_name(v));
        ctx->variables.emplace(v.uid(), p);
        ctx->builder->CreateMemSet(
            p, ctx->builder->getInt8(0),
            v.type()->size(), ::llvm::Align{16});
    }
    ctx->function.body()->accept(*this);
}

}// namespace luisa::compute::llvm
