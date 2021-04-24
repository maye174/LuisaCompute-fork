#include <ShaderCompile/HLSLCompiler.h>
#include <CJsonObject/CJsonObject.hpp>
#include <Utility/StringUtility.h>
#include <fstream>
#include <Utility/BinaryReader.h>
#include <File/Path.h>
#include <ShaderCompile/ShaderUniforms.h>
#include <Utility/BinaryReader.h>
namespace SCompile {

const luisa::compute::Context *HLSLCompiler::context;
std::filesystem::path HLSLCompiler::compiler_root;
std::filesystem::path HLSLCompiler::toolkit_root;

static constexpr bool g_needCommandOutput = false;
static const HANDLE g_hChildStd_IN_Rd = NULL;
static const HANDLE g_hChildStd_IN_Wr = NULL;
static const HANDLE g_hChildStd_OUT_Rd = NULL;
static const HANDLE g_hChildStd_OUT_Wr = NULL;
struct ProcessorData {
	_PROCESS_INFORMATION piProcInfo;
	bool bSuccess;
};
void CreateChildProcess(vengine::string const& cmd, ProcessorData* data) {
	if constexpr (g_needCommandOutput) {
		std::cout << cmd << std::endl;
		system(cmd.c_str());
		memset(data, 0, sizeof(ProcessorData));
		return;
	}

	PROCESS_INFORMATION piProcInfo;

	static HANDLE g_hInputFile = NULL;

	//PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	BOOL bSuccess = FALSE;

	// Set up members of the PROCESS_INFORMATION structure.

	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	// Set up members of the STARTUPINFO structure.
	// This structure specifies the STDIN and STDOUT handles for redirection.

	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = g_hChildStd_OUT_Wr;
	siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
	siStartInfo.hStdInput = g_hChildStd_IN_Rd;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

// Create the child process.
#ifdef UNICODE
	bSuccess = CreateProcess(NULL,
							 vengine::wstring(cmd).data(),// command line
							 NULL,						  // process security attributes
							 NULL,						  // primary thread security attributes
							 TRUE,						  // handles are inherited
							 0,							  // creation flags
							 NULL,						  // use parent's environment
							 NULL,						  // use parent's current directory
							 &siStartInfo,				  // STARTUPINFO pointer
							 &piProcInfo);				  // receives PROCESS_INFORMATION
#else
	bSuccess = CreateProcess(NULL,
							 cmd.data(),  // command line
							 NULL,		  // process security attributes
							 NULL,		  // primary thread security attributes
							 TRUE,		  // handles are inherited
							 0,			  // creation flags
							 NULL,		  // use parent's environment
							 NULL,		  // use parent's current directory
							 &siStartInfo,// STARTUPINFO pointer
							 &piProcInfo);// receives PROCESS_INFORMATION
#endif
	data->bSuccess = bSuccess;
	data->piProcInfo = piProcInfo;
}

void WaitChildProcess(ProcessorData* data)
// Create a child process that uses the previously created pipes for STDIN and STDOUT.
{

	// If an error occurs, exit the application.
	if (data->bSuccess) {
		auto&& piProcInfo = data->piProcInfo;
		// Close handles to the child process and its primary thread.
		// Some applications might keep these handles to monitor the status
		// of the child process, for example.
		WaitForSingleObject(piProcInfo.hProcess, INFINITE);
		WaitForSingleObject(piProcInfo.hThread, INFINITE);
		CloseHandle(piProcInfo.hProcess);
		CloseHandle(piProcInfo.hThread);
		// Close handles to the stdin and stdout pipes no longer needed by the child process.
		// If they are not explicitly closed, there is no way to recognize that the child process has ended.

		CloseHandle(g_hChildStd_OUT_Wr);
		CloseHandle(g_hChildStd_IN_Rd);
	}
}
vengine::string tempPath;

vengine::string fxcStart;
vengine::string dxcStart;
vengine::string shaderTypeCmd;
vengine::string funcName;
vengine::string output;
vengine::string macro_compile;
vengine::string dxcversion;
vengine::string dxcpath;
vengine::string fxcversion;
vengine::string fxcpath;
vengine::string pathFolder;
static spin_mutex outputMtx;
static vengine::vector<vengine::string> errorMessage;

enum class Compiler : bool {
	DXC = false,
	FXC = true
};

Compiler computeCompilerUsage;
Compiler rasterizeCompilerUsage;
Compiler rayTracingCompilerUsage;
std::atomic_bool inited = false;
void HLSLCompiler::InitRegisterData(const luisa::compute::Context &ctx) {
	
	context = &ctx;
	compiler_root = std::filesystem::canonical(ctx.runtime_directory() / "backends" / "VEngineCompiler");
	toolkit_root = compiler_root / "CompilerToolkit";
	LUISA_INFO("DirectX compiler root: {}.", compiler_root.string());
	
	if (inited.exchange(true)) return;
	shaderTypeCmd = " /T ";
	funcName = " /E ";
	output = " /Fo ";
	macro_compile = " /D ";
	using namespace neb;
	std::unique_ptr<CJsonObject> obj(ReadJson((toolkit_root / "register.json").string().c_str()));
	if (!obj) {
		std::cout << "Register.json not found in HLSLCompiler folder!"_sv << std::endl;
		system("pause");
		exit(0);
	}
	vengine::string value;
	CJsonObject sonObj;
	tempPath = context->cache_directory().string().c_str();
	pathFolder = context->cache_directory().string().c_str();
	auto GenerateSettings = [&](vengine::string& settings) -> void {
		settings.clear();
		int sz = sonObj.GetArraySize();
		static vengine::string SplitString = " /"_sv;
		for (int i = 0; i < sz; ++i) {
			if (sonObj.Get(i, value))
				settings += SplitString + value;
		}
	};
	if (obj->Get("DXC"_sv, sonObj)) {
		if (sonObj.Get("SM"_sv, value)) {
			dxcversion = value;
		}
		if (sonObj.Get("Path"_sv, value)) {
			dxcpath = (toolkit_root / value.c_str()).string().c_str();
//			dxcpath = vengine::string("HLSLCompiler\\"_sv) + value;
		}
		if (sonObj.Get("Settings"_sv, sonObj) && sonObj.IsArray()) {
			GenerateSettings(dxcStart);
		}
	}
	if (obj->Get("FXC"_sv, sonObj)) {
		if (sonObj.Get("SM"_sv, value)) {
			fxcversion = value;
		}
		if (sonObj.Get("Path"_sv, value)) {
			fxcpath = (toolkit_root / value.c_str()).string().c_str();
//			fxcpath = vengine::string("HLSLCompiler\\"_sv) + value;
		}
		if (sonObj.Get("Settings"_sv, sonObj) && sonObj.IsArray()) {
			GenerateSettings(fxcStart);
		}
	}
	if (obj->Get("CompilerUsage"_sv, sonObj)) {
		if (sonObj.Get("Rasterize"_sv, value)) {
			StringUtil::ToLower(value);
			rasterizeCompilerUsage = (Compiler)(value == "fxc");
		}
		if (sonObj.Get("RayTracing"_sv, value)) {
			StringUtil::ToLower(value);
			rayTracingCompilerUsage = (Compiler)(value == "fxc");
		}
		if (sonObj.Get("Compute"_sv, value)) {
			StringUtil::ToLower(value);
			computeCompilerUsage = (Compiler)(value == "fxc");
		}
	}
//	if (obj->Get("TempFolder"_sv, value)) {
//		tempPath = value;
//		Path(tempPath).TryCreateDirectory();
//	}
//	if (obj->Get("CompileResultFolder"_sv, pathFolder)) {
//		if (!pathFolder.empty()) {
//			char lst = pathFolder.end()[-1];
//			if (lst != '/' && lst != '\\') {
//				pathFolder += '/';
//			}
//		}
//	} else
//		pathFolder.clear();
}
struct CompileFunctionCommand {
	vengine::string name;
	ShaderType type;
};
void GenerateCompilerCommand(
	vengine::string const& fileName,
	vengine::string const& functionName,
	vengine::string const& resultFileName,
	ShaderType shaderType,
	Compiler compiler,
	vengine::string& cmdResult) {
	vengine::string const* compilerPath = nullptr;
	vengine::string const* compileShaderVersion = nullptr;
	vengine::string const* start = nullptr;
	switch (compiler) {
		case Compiler::FXC:
			compilerPath = &fxcpath;
			compileShaderVersion = &fxcversion;
			start = &fxcStart;
			break;
		case Compiler::DXC:
			compilerPath = &dxcpath;
			compileShaderVersion = &dxcversion;
			start = &dxcStart;
			break;
		default:
			std::cout << "Unsupported Compiler!"_sv << std::endl;
			return;
	}
	vengine::string shaderTypeName;
	switch (shaderType) {
		case ShaderType::ComputeShader:
			shaderTypeName = "cs_"_sv;
			break;
		case ShaderType::VertexShader:
			shaderTypeName = "vs_"_sv;
			break;
		case ShaderType::HullShader:
			shaderTypeName = "hs_"_sv;
			break;
		case ShaderType::DomainShader:
			shaderTypeName = "ds_"_sv;
			break;
		case ShaderType::GeometryShader:
			shaderTypeName = "gs_"_sv;
			break;
		case ShaderType::PixelShader:
			shaderTypeName = "ps_"_sv;
			break;
		case ShaderType::RayTracingShader:
			shaderTypeName = "lib_"_sv;
			break;
		default:
			shaderTypeName = " "_sv;
			break;
	}
	shaderTypeName += *compileShaderVersion;
	cmdResult.clear();
	cmdResult.reserve(50);
	std::cout << "FFF " << shaderTypeCmd << std::endl;
	cmdResult << *compilerPath << *start << shaderTypeCmd << shaderTypeName;
	if (!functionName.empty()) {
		cmdResult += funcName;
		cmdResult += functionName;
	}
	cmdResult += output;
	cmdResult += resultFileName;
	cmdResult += " "_sv;
	cmdResult += fileName;
}

template<typename T>
void PutIn(vengine::vector<char>& c, const T& data) {
	T* cc = &((T&)data);
	uint64 siz = c.size();
	c.resize(siz + sizeof(T));
	memcpy(c.data() + siz, cc, sizeof(T));
}
void PutIn(vengine::vector<char>& c, void* data, uint64 dataSize) {
	if (dataSize == 0) return;
	uint64 siz = c.size();
	c.resize(siz + dataSize);
	memcpy(c.data() + siz, data, dataSize);
}
template<>
void PutIn<vengine::string>(vengine::vector<char>& c, vengine::string const& data) {
	PutIn<uint>(c, (uint)data.length());
	uint64 siz = c.size();
	c.resize(siz + data.length());
	memcpy(c.data() + siz, data.data(), data.length());
}
template<typename T>
void DragData(std::ifstream& ifs, T& data) {
	ifs.read((char*)&data, sizeof(T));
}
template<>
void DragData<vengine::string>(std::ifstream& ifs, vengine::string& str) {
	uint32_t length = 0;
	DragData<uint32_t>(ifs, length);
	str.clear();
	str.resize(length);
	ifs.read(str.data(), length);
}

void PutInSerializedObjectAndData(
	vengine::vector<char> const& serializeObj,
	vengine::vector<char>& resultData,
	vengine::vector<ShaderVariable> vars) {
	PutIn<uint64_t>(resultData, serializeObj.size());
	PutIn(resultData, serializeObj.data(), serializeObj.size());

	PutIn<uint>(resultData, (uint)vars.size());
	for (auto i = vars.begin(); i != vars.end(); ++i) {
		PutIn<vengine::string>(resultData, i->name);
		PutIn<ShaderVariableType>(resultData, i->type);
		PutIn<uint>(resultData, i->tableSize);
		PutIn<uint>(resultData, i->registerPos);
		PutIn<uint>(resultData, i->space);
	}
}

bool HLSLCompiler::ErrorHappened() {
	return !errorMessage.empty();
}

void HLSLCompiler::PrintErrorMessages() {
	for (auto& msg : errorMessage) {
		std::cout << msg << '\n';
	}
	errorMessage.clear();
}

void HLSLCompiler::CompileShader(
	vengine::string const& fileName,
	vengine::vector<ShaderVariable> const& vars,
	vengine::vector<PassDescriptor> const& passDescs,
	vengine::vector<char> const& customData,
	vengine::vector<char>& resultData) {
	resultData.clear();
	resultData.reserve(65536);
	PutInSerializedObjectAndData(
		customData,
		resultData,
		vars);
	auto func = [&](ProcessorData* pData, vengine::string const& str) -> bool {
		//TODO
		uint64_t fileSize;
		WaitChildProcess(pData);
		//CreateChildProcess(,);
		//system(command.c_str());
		fileSize = 0;
		std::ifstream ifs(str.data(), std::ios::binary);
		if (!ifs) return false;
		ifs.seekg(0, std::ios::end);
		fileSize = ifs.tellg();
		ifs.seekg(0, std::ios::beg);
		PutIn<uint64_t>(resultData, fileSize);
		if (fileSize == 0) return false;
		uint64 originSize = resultData.size();
		resultData.resize(fileSize + originSize);
		ifs.read(resultData.data() + originSize, fileSize);
		return true;
	};
	HashMap<vengine::string, std::pair<ShaderType, uint>> passMap(passDescs.size() * 2);
	for (auto i = passDescs.begin(); i != passDescs.end(); ++i) {
		auto findFunc = [&](vengine::string const& namestr, ShaderType type) -> void {
			if (namestr.empty()) return;
			if (!passMap.Contains(namestr)) {
				passMap.Insert(namestr, std::pair<ShaderType, uint>(type, (uint)passMap.Size()));
			}
		};
		findFunc(i->vertex, ShaderType::VertexShader);
		findFunc(i->hull, ShaderType::HullShader);
		findFunc(i->domain, ShaderType::DomainShader);
		findFunc(i->fragment, ShaderType::PixelShader);
	}
	vengine::vector<CompileFunctionCommand> functionNames(passMap.Size());
	PutIn<uint>(resultData, (uint)passMap.Size());
	passMap.IterateAll([&](vengine::string const& key, std::pair<ShaderType, uint>& value) -> void {
		CompileFunctionCommand cmd;
		cmd.name = key;
		cmd.type = value.first;
		functionNames[value.second] = std::move(cmd);
	});
	vengine::string commandCache;
	ProcessorData data;
	vengine::vector<vengine::string> strs(functionNames.size());
	for (uint i = 0; i < functionNames.size(); ++i) {
		strs[i] = tempPath + vengine::to_string(i);
		GenerateCompilerCommand(
			fileName, functionNames[i].name, strs[i], functionNames[i].type, rasterizeCompilerUsage, commandCache);
		CreateChildProcess(commandCache, &data);
		if (!func(&data, strs[i])) {
			std::lock_guard<spin_mutex> lck(outputMtx);
			errorMessage.emplace_back(std::move(vengine::string("Shader "_sv) + fileName + " Failed!"_sv));
			return;
		}
	}

	PutIn<uint>(resultData, (uint)passDescs.size());
	for (auto i = passDescs.begin(); i != passDescs.end(); ++i) {
		PutIn(resultData, i->name);
		PutIn(resultData, i->rasterizeState);
		PutIn(resultData, i->depthStencilState);
		PutIn(resultData, i->blendState);
		auto PutInFunc = [&](vengine::string const& value) -> void {
			if (value.empty() || !passMap.Contains(value))
				PutIn<int>(resultData, -1);
			else
				PutIn<int>(resultData, (int)passMap[value].second);
		};
		PutInFunc(i->vertex);
		PutInFunc(i->hull);
		PutInFunc(i->domain);
		PutInFunc(i->fragment);
	}
	for (auto i = strs.begin(); i != strs.end(); ++i)
		remove(i->c_str());
}
void HLSLCompiler::CompileComputeShader(
	vengine::string const& fileName,
	vengine::vector<ShaderVariable> const& vars,
	vengine::string const& passDesc,
	vengine::vector<char> const& customData,
	vengine::vector<char>& resultData) {
	resultData.clear();
	resultData.reserve(65536);
	PutInSerializedObjectAndData(
		customData,
		resultData,
		vars);

	auto func = [&](vengine::string const& str, ProcessorData* data) -> bool {
		uint64_t fileSize;
		WaitChildProcess(data);
		//CreateChildProcess(command);
		//TODO
		//system(command.c_str());
		fileSize = 0;
		BinaryReader ifs(str);
		if (!ifs) return false;
		fileSize = ifs.GetLength();
		PutIn<uint64_t>(resultData, fileSize);
		if (fileSize == 0) return false;
		uint64 originSize = resultData.size();
		resultData.resize(fileSize + originSize);
		ifs.Read(resultData.data() + originSize, fileSize);
		return true;
	};
	vengine::string kernelCommand;

	PutIn<uint>(resultData, 1);
	static std::atomic_uint temp_count = 0;
	vengine::string tempFile = tempPath;
	tempFile << vengine::to_string(temp_count++)
			 << ".obj"_sv;
	GenerateCompilerCommand(
		fileName,
		passDesc,
		tempFile,
		ShaderType::ComputeShader,
		computeCompilerUsage,
		kernelCommand);
	ProcessorData data;
	CreateChildProcess(kernelCommand, &data);
	if (!func(tempFile, &data)) {
		std::lock_guard<spin_mutex> lck(outputMtx);
		std::cout << vengine::string("ComputeShader "_sv) + fileName + " Failed!"_sv << std::endl;

		return;
	}

	PutIn<uint>(resultData, 1);
	PutIn(resultData, passDesc);
	PutIn<uint>(resultData, 0);
	remove(tempFile.c_str());
}
void HLSLCompiler::CompileDXRShader(
	vengine::string const& fileName,
	vengine::vector<ShaderVariable> const& vars,
	CompileDXRHitGroup const& passDescs,
	uint64 raypayloadMaxSize,
	uint64 recursiveCount,
	vengine::vector<char> const& customData,
	vengine::vector<char>& resultData) {
	resultData.clear();
	resultData.reserve(65536);
	if (raypayloadMaxSize == 0) {
		std::lock_guard<spin_mutex> lck(outputMtx);
		std::cout << "Raypayload Invalid! \n"_sv;
		errorMessage.emplace_back(std::move(
			vengine::string("DXRShader "_sv) + fileName + " Failed!"_sv));
		return;
	}
	PutInSerializedObjectAndData(
		customData,
		resultData,
		vars);
	PutIn<uint64>(resultData, recursiveCount);
	PutIn<uint64>(resultData, raypayloadMaxSize);
	PutIn<vengine::string>(resultData, passDescs.name);
	PutIn<uint>(resultData, passDescs.shaderType);
	for (auto& func : passDescs.functions) {
		PutIn<vengine::string>(resultData, func);
	}

	auto func = [&](vengine::string const& str, ProcessorData* data) -> bool {
		uint64_t fileSize;
		WaitChildProcess(data);
		//CreateChildProcess(command);
		//TODO
		//system(command.c_str());
		fileSize = 0;
		std::ifstream ifs(str.data(), std::ios::binary);
		if (!ifs) return false;
		ifs.seekg(0, std::ios::end);
		fileSize = ifs.tellg();
		ifs.seekg(0, std::ios::beg);
		PutIn<uint64_t>(resultData, fileSize);
		if (fileSize == 0) return false;
		uint64 originSize = resultData.size();
		resultData.resize(fileSize + originSize);
		ifs.read(resultData.data() + originSize, fileSize);
		return true;
	};
	vengine::string kernelCommand;
	GenerateCompilerCommand(
		fileName, vengine::string(), tempPath, ShaderType::RayTracingShader, rayTracingCompilerUsage, kernelCommand);
	ProcessorData data;
	CreateChildProcess(kernelCommand, &data);
	if (!func(tempPath, &data)) {
		std::lock_guard<spin_mutex> lck(outputMtx);
		errorMessage.emplace_back(std::move(
			vengine::string("DXRShader "_sv) + fileName + " Failed!"_sv));
	}
	remove(tempPath.c_str());
}

void HLSLCompiler::GetShaderVariables(
	luisa::compute::Function const& func,
	vengine::vector<ShaderVariable>& result) {

	uint registPos = 0;
	uint rwRegistPos = 0;
	//CBuffer
	result.emplace_back(
		"Params"_sv,
		ShaderVariableType::ConstantBuffer,
		1,
		0,
		0);

	//Buffer
	for (auto& i : func.captured_buffers()) {
		auto& buffer = i.variable;
		vengine::string name = 'v' + vengine::to_string(buffer.uid());
		if (((uint)func.variable_usage(i.variable.uid()) & (uint)luisa::compute::Variable::Usage::WRITE) != 0) {
			auto& var = result.emplace_back(
				std::move(name),
				ShaderVariableType::RWStructuredBuffer,
				1,
				rwRegistPos,
				0);
			rwRegistPos++;
		} else {
			auto& var = result.emplace_back(
				std::move(name),
				ShaderVariableType::StructuredBuffer,
				1,
				registPos,
				0);
			registPos++;

		}
	}
	for (auto& i : func.captured_textures()) {
		auto& buffer = i.variable;
		vengine::string name = 'v' + vengine::to_string(buffer.uid());
		if (((uint)func.variable_usage(i.variable.uid()) & (uint)luisa::compute::Variable::Usage::WRITE) != 0) {
			auto& var = result.emplace_back(
				std::move(name),
				ShaderVariableType::UAVDescriptorHeap,
				1,
				rwRegistPos,
				0);
			rwRegistPos++;
		} else {
			auto& var = result.emplace_back(
				std::move(name),
				ShaderVariableType::SRVDescriptorHeap,
				1,
				registPos,
				0);
			registPos++;
		}
	}
}
bool HLSLCompiler::CheckNeedReCompile(std::array<uint8_t, 16> const& md5, vengine::string const& shaderFileName) {
	vengine::string md5Path = shaderFileName + ".md5"_sv;
	{
		BinaryReader binReader(md5Path);
		if (binReader && binReader.GetLength() >= md5.size()) {
			std::array<uint64, 2> file;
			binReader.Read((char*)file.data(), md5.size());
			uint64 const* compare = (uint64 const*)md5.data();
			if (compare[0] == file[0] && compare[1] == file[1]) return true;
		}
	}
	std::ofstream ofs(md5Path.c_str(), std::ios::binary);
	ofs.write((char const*)md5.data(), md5.size());
	return false;
}
}// namespace SCompile
