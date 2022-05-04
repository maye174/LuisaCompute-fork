//
// Created by Mike Smith on 2021/4/6.
//

#include <stb/stb_image_write.h>

#include <runtime/context.h>
#include <runtime/device.h>
#include <runtime/stream.h>
#include <dsl/syntax.h>
#include <dsl/sugar.h>
#include <dsl/printer.h>

using namespace luisa;
using namespace luisa::compute;

int main(int argc, char *argv[]) {

    log_level_verbose();

    Context context{argv[0]};
    auto device = context.create_device("ispc");
    Printer printer{device};

    // __device__
    Callable linear_to_srgb = [&](Float3 linear) noexcept {
        auto x = linear.xyz();
        $if(all(dispatch_id() <= make_uint3(33, 0, 0))) {
            printer.verbose_with_location("Linear: ({}, {}, {})", x.x, x.y, x.z);
        };
        auto srgb = make_uint3(
            round(saturate(
                      select(1.055f * pow(x, 1.0f / 2.4f) - 0.055f,
                             12.92f * x,
                             x <= 0.00031308f)) *
                  255.0f));
        return (255u << 24u) | (srgb.z << 16u) | (srgb.y << 8u) | srgb.x;
    };

    // __global__
    // void fill_image(uint *image) {
    //   auto dis_id = blockIdx * blockDim + threadIdx;
    //   auto coord = make_uint2(dis_id.x, dis_id.y);
    //   auto rg = float2(coord) / float2(dis_size.xy());
    //   image[coord.x + coord.y  * dis_size.x] = value;
    // }
    Kernel2D fill_image_kernel = [&](BufferUInt image) noexcept {
        auto coord = dispatch_id().xy();
        auto rg = make_float2(coord) / make_float2(dispatch_size().xy());
        $if(all(dispatch_id() == make_uint3(12, 0, 0))) {
            printer.info("{}{}{}Hello, coord = ({}, {})", 1, 1.f, true, coord.x, coord.y);
        };
        image.write(coord.x + coord.y * dispatch_size_x(), linear_to_srgb(make_float3(rg, 0.5f)));
    };

    std::vector<std::byte> download_image(1024u * 1024u * 4u);

    // cuMemAlloc
    auto buffer = device.create_buffer<uint>(1024 * 1024);

    // cuStreamCreate
    auto stream = device.create_stream();
    stream << printer.reset();

    // dispatch
    stream << fill_image_kernel(device, buffer).dispatch(1024u, 1024u)
           << buffer.copy_to(download_image.data())
           << printer.retrieve()
           << synchronize();
    stbi_write_png("result.png", 1024u, 1024u, 4u, download_image.data(), 0u);
}
