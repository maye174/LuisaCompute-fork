//
// Created by Mike Smith on 2021/2/27.
//

#include <iostream>
#include <chrono>
#include <numeric>

#include <core/dynamic_module.h>
#include <runtime/device.h>
#include <runtime/context.h>
#include <ast/interface.h>
#include <compile/cpp_codegen.h>
#include <dsl/syntax.h>

#include <tests/fake_device.h>
#include <backends/dx/Common/FunctorMeta.h>
using namespace luisa;
using namespace luisa::compute;
using namespace luisa::compute::dsl;
using namespace luisa::compute::compile;

struct Test {
    int3 something;
    float a;
};

template<typename T>
struct GetKernelArgType;

template<typename T>
struct GetKernelArgType<Var<T>> {
    using Type = typename T;
};
template<typename T>
struct GetKernelArgType<BufferView<T>> {
    using Type = typename Buffer<T>;
};

template<typename T>
struct GetKernelType;

template<typename Ret, typename... Args>
struct GetKernelType<Ret(Args...)> {
    using KernelType = typename Ret(GetKernelArgType<Args>::Type...);
};

template<typename Func>
decltype(auto) CompileKernel(Func &&func) {
    using FuncType = MFunctorType<std::remove_cvref_t<Func>>;//void(BufferView<float>, Var<uint>)
    return Kernel<GetKernelType<FuncType>::KernelType>(func);
}

LUISA_STRUCT(Test, something, a)

int main(int argc, char *argv[]) {

    luisa::log_level_verbose();

    Context context{argv[0]};
    FakeDevice device{context};

    auto buffer = device.create_buffer<float4>(1024u);
    auto float_buffer = device.create_buffer<float>(1024u);

    std::vector<int> const_vector(128u);
    std::iota(const_vector.begin(), const_vector.end(), 0);

    Callable add_mul = [](Var<int> a, Var<int> b) noexcept {
        return std::make_tuple(a + b, a * b);
    };

    std::function callable = LUISA_CALLABLE(Var<int> a, Var<int> b, Var<float> c) noexcept {
        Constant int_consts = const_vector;
        return cast<float>(a) + int_consts[b].cast<float>() * c;
    };

    // binding to template lambdas
    Callable<int(int, int)> add = [&]<typename T>(Var<T> a, Var<T> b) noexcept {
        return a + b;
    };

    auto t0 = std::chrono::high_resolution_clock::now();
    Constant float_consts = {1.0f, 2.0f};
    Constant int_consts = const_vector;

    // With C++17's deduction guides, omitting template arguments here is also supported, i.e.
    // >>> Kernel kernel = [&](...) { ... }
    //auto func = [&](BufferView<float> buffer_float, Var<uint> count) noexcept {
    Kernel<void(Buffer<float>, uint)> kernel = CompileKernel([&](BufferView<float> buffer_float, Var<uint> count) noexcept -> void {
        Shared<float4> shared_floats{16};

        Var v_int = 10;

        auto [a, m] = add_mul(v_int, v_int);
        Var a_copy = a;
        Var m_copy = m;

        for (auto v : range(v_int)) {
            v_int += v;
        }

        Var v_int_add_one = add(v_int, 1);
        Var vv_int = int_consts[v_int];
        Var v_float = buffer_float[count + thread_id().x];
        Var vv_float = float_consts[vv_int];
        Var call_ret = callable(10, v_int, v_float);

        Var v_float_copy = v_float;

        Var z = -1 + v_int * v_float + 1.0f;
        z += 1;
        static_assert(std::is_same_v<decltype(z), Var<float>>);
        for (uint i = 0; i < 3; ++i) {
            Var v_vec = float3{1.0f};
            Var v2 = float3{2.0f} - v_vec * 2.0f;
            v2 *= 5.0f + v_float;

            Var<float2> w{v_int, v_float};
            w *= float2{1.2f};

            if_(1 + 1 == 2, [] {
                Var a = 0.0f;
            }).elif (1 + 2 == 3, [] {
                  Var b = 1.0f;
              }).else_([] {
                Var c = 2.0f;
            });

            while_(true, [&] {
                z += 1;
            });

            switch_(123)
                .case_(1, [] {

                })
                .case_(2, [] {

                })
                .default_([] {

                });

            Var x = w.x;
        }

        Var<int3> s;
        Var<Test> vvt{s, v_float_copy};
        Var<Test> vt{vvt};

        Var vt_copy = vt;
        Var c = 0.5f + vt.a * 1.0f;

        Var vec4 = buffer[10];           // indexing into captured buffer (with literal)
        Var another_vec4 = buffer[v_int];// indexing into captured buffer (with Var)*/
        buffer[v_int + 1] = 123;
    });
    auto t1 = std::chrono::high_resolution_clock::now();

    auto command = kernel(float_buffer, 12u).parallelize(1024u);
    auto launch_command = static_cast<KernelLaunchCommand *>(command.get());
    LUISA_INFO("Command: kernel = {}, args = {}", launch_command->kernel_uid(), launch_command->argument_count());
    auto function = Function::kernel(launch_command->kernel_uid());

#if defined(LUISA_BACKEND_DX_ENABLED)
    DynamicModule dll{std::filesystem::canonical(argv[0]).parent_path() / "backends", "luisa-compute-backend-dx"};
    auto hlsl_serialize = dll.function<void(Function)>("SerializeMD5");
    auto hlsl_codegen = dll.function<void(Function)>("CodegenBody");
    // hlsl_serialize(function);
    hlsl_codegen(function);
#else
    auto t2 = std::chrono::high_resolution_clock::now();
    Codegen::Scratch scratch;
    CppCodegen codegen{scratch};
    codegen.emit(function);
    auto t3 = std::chrono::high_resolution_clock::now();

    std::cout << scratch.view() << std::endl;

    using namespace std::chrono_literals;
    LUISA_INFO("AST: {} ms, Codegen: {} ms", (t1 - t0) / 1ns * 1e-6, (t3 - t2) / 1ns * 1e-6);
#endif
}
