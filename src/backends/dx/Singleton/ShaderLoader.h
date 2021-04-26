#pragma once
#include <Common/GFXUtil.h>
#include <Common/HashMap.h>
#include <CJsonObject/CJsonObject.hpp>
#include <JobSystem/JobInclude.h>

class ComputeShader;
class RayShader;
class ShaderLoaderGlobal;
namespace luisa::compute {
class DXDevice;
}

class ShaderLoader {
	friend class luisa::compute::DXDevice;

private:
	static thread_local ShaderLoaderGlobal* current;
	static int32_t shaderIDCount;
	static RayShader* LoadRayShader(const vengine::string& name, GFXDevice* device, const vengine::string& path);
	static ComputeShader* LoadComputeShader(const vengine::string& name, GFXDevice* device, const vengine::string& path);
	//static void CompileShaders(GFXDevice* device, JobBucket* bucket, const vengine::string& path);
	ShaderLoader() = delete;

public:
	static RayShader const* GetRayShader(const vengine::string& name);
	static ComputeShader const* GetComputeShader(const vengine::string& name);
	static void ReleaseComputeShader(ComputeShader const* shader);
	static ShaderLoaderGlobal* Init(GFXDevice* device);
	static void Reload(GFXDevice* device, JobBucket* bucket);
	static void Dispose(ShaderLoaderGlobal* glb);
	KILL_COPY_CONSTRUCT(ShaderLoader)
};