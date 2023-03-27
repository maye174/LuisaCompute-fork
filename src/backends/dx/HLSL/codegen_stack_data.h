#pragma once
#include <vstl/common.h>
#include "dx_codegen.h"
#include "struct_generator.h"
namespace lc::dx {

struct CodegenStackData : public vstd::IOperatorNewBase {
    luisa::compute::Function kernel;
    vstd::unordered_map<Type const *, uint64> structTypes;
    vstd::unordered_map<uint64, uint64> constTypes;
    vstd::unordered_map<void const *, uint64> funcTypes;
    vstd::unordered_map<Type const *, vstd::unique_ptr<StructGenerator>> customStruct;
    vstd::unordered_map<uint, uint> arguments;
    vstd::unordered_map<Type const*, vstd::string> internalStruct;
    enum class FuncType : uint8_t {
        Kernel,
        Vert,
        Pixel,
        Callable
    };
    FuncType funcType;
    bool isRaster = false;
    bool isPixelShader = false;
    bool pixelFirstArgIsStruct = false;
    uint64 count = 0;
    uint64 constCount = 0;
    uint64 funcCount = 0;
    uint64 tempCount = 0;
    uint64 bindlessBufferCount = 0;
    uint64 structCount = 0;
    uint64 argOffset = 0;
    int64_t appdataId = -1;
    int64 scopeCount = -1;

    vstd::function<void (Type const *)> generateStruct;
    vstd::unordered_map<vstd::string, vstd::string, vstd::hash<vstd::StringBuilder>> structReplaceName;
    vstd::unordered_map<uint64, Variable> sharedVariable;
    Expression const *tempSwitchExpr;
    size_t tempSwitchCounter = 0;
    CodegenStackData();
    void Clear();
    void AddBindlessType(Type const *type);
    vstd::string_view CreateStruct(Type const *t);
    std::pair<uint64, bool> GetConstCount(uint64 data);
    uint64 GetFuncCount(void const *data);
    uint64 GetTypeCount(Type const *t);
    ~CodegenStackData();
    static vstd::unique_ptr<CodegenStackData> Allocate();
    static void DeAllocate(vstd::unique_ptr<CodegenStackData> &&v);
    // static bool& ThreadLocalSpirv();
};
}// namespace lc::dx