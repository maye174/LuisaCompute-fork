add_rules("mode.release", "mode.debug")
-- in case bug from version control
set_policy("build.ccache", false)
-- options
option("use_mimalloc")
set_values(true, false)
set_default(true)
set_showmenu(true)
option_end()

option("use_unity_build")
set_values(true, false)
set_default(true)
set_showmenu(true)
option_end()

option("enable_simd")
set_values(true, false)
set_default(true)
set_showmenu(true)
option_end()

option("dx_backend")
set_values(true, false)
set_default(true)
set_showmenu(true)
option_end()

option("cuda_backend")
set_values(true, false)
set_default(true)
set_showmenu(true)
option_end()

option("metal_backend")
set_values(true, false)
set_default(true)
set_showmenu(true)
option_end()

option("cpu_backend")
set_values(true, false)
set_default(false)
set_showmenu(true)
option_end()

option("export_config")
set_values(true, false)
set_default(false)
set_showmenu(true)
option_end()

option("enable_tools")
set_values(true, false)
set_default(false)
set_showmenu(true)
option_end()

option("enable_tests")
set_values(true, false)
set_default(false)
set_showmenu(true)
option_end()

option("enable_py")
set_values(true, false)
set_default(false)
set_showmenu(true)
option_end()

option("enable_rust")
set_values(true, false)
set_default(false)
set_showmenu(true)
option_end()

option("enable_dsl")
set_values(true, false)
set_default(false)
set_showmenu(true)
option_end()

option("enable_gui")
set_values(true, false)
set_default(false)
set_showmenu(true)
option_end()
-- options

option("legal_env")
set_showmenu(false)
add_csnippets("legal_env", "return ((sizeof(void*)==8)&&(sizeof(int)==4)&&(sizeof(char)==1))?0:-1;", {
	tryrun = true
})
option_end()
rule("check_env")
set_kind("project")
before_build(function()
	if not has_config("legal_env") then
		utils.error("Illegal environment! Please check your compiler, architecture or platform!")
		return
	end
end)
rule_end()
add_rules("check_env")
if has_config("legal_env") then
	UseMimalloc = get_config("use_mimalloc")
	UseUnityBuild = get_config("use_unity_build")
	ExportConfig = (get_config("export_config"))
	UseSIMD = get_config("enable_simd")
	-- test require dsl
	EnableTest = get_config("enable_tests")
	EnableDSL = get_config("enable_dsl")
	DxBackend = get_config("dx_backend") and is_plat("windows")
	-- TODO: require environment check
	CudaBackend = get_config("cuda_backend") and (is_plat("windows") or is_plat("linux")) and false
	MetalBackend = get_config("metal_backend") and is_plat("macos")
	CpuBackend = get_config("cpu_backend")
	-- TODO: rust condition
	EnableRust = get_config("enable_rust") or CudaBackend or MetalBackend or CpuBackend
	EnableGUI = get_config("enable_gui")

	if is_mode("debug") then
		set_targetdir("bin/debug")
	else
		set_targetdir("bin/release")
	end

	includes("xmake_func.lua")
	includes("src")
	if ExportConfig then
		add_rules("export_define_project")
	end
end
