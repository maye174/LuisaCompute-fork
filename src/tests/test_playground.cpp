//
// Created by Mike Smith on 2021/4/6.
//

#include <stb/stb_image_write.h>

#include <runtime/context.h>
#include <runtime/device.h>
#include <runtime/stream.h>
#include <dsl/syntax.h>
#include <dsl/printer.h>

using namespace luisa;
using namespace luisa::compute;

struct M {
    float4x4 m;
};

LUISA_STRUCT(M, m) {};

template<typename T, typename = void>
struct test : std::false_type {};

template<typename T>
struct test<T, std::void_t<decltype(T::inputLayouts)>> : std::true_type {};

struct WithLayouts { inline static int inputLayouts; };
struct WithoutLayouts {};

static_assert(test<WithLayouts>::value);
static_assert(!test<WithoutLayouts>::value);

int main(int argc, char *argv[]) {

    log_level_verbose();

    Context context{argv[0]};
    auto device = context.create_device("dx");
    auto m4 = make_float4x4(
        1.f, 2.f, 3.f, 4.f,
        5.f, 6.f, 7.f, 8.f,
        9.f, 10.f, 11.f, 12.f,
        13.f, 14.f, 15.f, 16.f);
    auto buffer = device.create_buffer<float4x4>(1u);
    auto stream = device.create_stream();
    auto scalar_buffer = device.create_buffer<float>(16u);

    Kernel1D test_kernel = [&](Float4x4 m) {
        set_block_size(1u, 1u, 1u);
        auto m2 = buffer.read(0u);
        for (auto i = 0u; i < 4u; i++) {
            for (auto j = 0u; j < 4u; j++) {
                scalar_buffer.write(i * 4u + j, m[i][j]);
            }
        }
    };
    auto test = device.compile(test_kernel);

    // dispatch
    std::array<float, 16u> download{};
    stream << buffer.copy_from(&m4)
           << test(m4).dispatch(1u)
           << scalar_buffer.copy_to(download.data())
           << synchronize();
    std::cout << "buffer:";
    for (auto i : std::span{download}.subspan(0u, 16u)) { std::cout << " " << i; }
    std::cout << std::endl;
}
