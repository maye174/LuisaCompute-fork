set_xmakever("2.7.8")
add_rules("mode.release", "mode.debug", "mode.releasedbg")
-- disable ccache in-case error
set_policy("build.ccache", false)
-- pre-defined options
-- enable mimalloc as default allocator: https://github.com/LuisaGroup/mimalloc
option("enable_mimalloc")
set_values(true, false)
set_default(true)
set_showmenu(true)
option_end()
-- enable unity(jumbo) build, enable this option will optimize compile speed
option("enable_unity_build")
set_values(true, false)
set_default(true)
set_showmenu(true)
option_end()
-- enable sse and sse2 SIMD
option("enable_simd")
set_values(true, false)
set_default(true)
set_showmenu(true)
option_end()
-- enable DirectX-12 backend
option("dx_backend")
set_values(true, false)
set_default(true)
set_showmenu(true)
option_end()
-- enable Vulkan backend
option("vk_backend")
set_values(true, false)
set_default(true)
set_showmenu(true)
option_end()
-- enable NVIDIA-CUDA backend
option("cuda_backend")
set_values(true, false)
set_default(true)
set_showmenu(true)
option_end()
-- enable Metal backend
option("metal_backend")
set_values(true, false)
set_default(true)
set_showmenu(true)
option_end()
-- enable cpu backend
option("cpu_backend")
set_values(true, false)
set_default(false)
set_showmenu(true)
option_end()
-- enable tests module
option("enable_tests")
set_values(true, false)
set_default(false)
set_showmenu(true)
option_end()
-- python include path
option("py_include")
set_default(false)
set_showmenu(true)
option_end()
-- python include path
option("py_linkdir")
set_default(false)
set_showmenu(true)
option_end()
-- python include path
option("py_libs")
set_default(false)
set_showmenu(true)
option_end()
-- enable intermediate representation module (rust required)
option("enable_ir")
set_values(true, false)
set_default(false)
set_showmenu(true)
option_end()
-- enable c-language api module for cross-language bindings module
option("enable_api")
set_values(true, false)
set_default(false)
set_showmenu(true)
option_end()
-- enable C++ DSL module
option("enable_dsl")
set_values(true, false)
set_default(false)
set_showmenu(true)
option_end()
-- enable GUI module
option("enable_gui")
set_values(true, false)
set_default(false)
set_showmenu(true)
option_end()
-- custom bin dir
option("bin_dir")
set_default("bin")
set_showmenu(true)
option_end()

includes("options.lua")
if lc_config then
	for k, v in pairs(lc_config) do
		set_config(k, v)
	end
end
-- options lua
-- pre-defined options end
if is_arch("x64", "x86_64", "arm64") then
	-- test require dsl
	-- get_config("dx_backend") = get_config("dx_backend") and is_plat("windows")
	includes("xmake_func.lua")
	local bin_dir = get_config("bin_dir")
	if (bin_dir) then
		set_targetdir(bin_dir)
	end
	includes("src")
else
	target("_lc_illegal_env")
	set_kind("phony")
	on_load(function(target)
		utils.error("Illegal environment. Please check your compiler, architecture or platform.")
	end)
	target_end()
end
