target("lc-ir")
_config_project({
	project_kind = "shared"
})
on_load(function(target)
	local function rela(p)
		return path.relative(path.absolute(p, os.scriptdir()), os.projectdir())
	end
	target:add("defines", "LC_IR_EXPORT_DLL")
	target:add("deps", "lc-ast", "lc-rust")
	if is_plat("windows") then
		target:add("syslinks", "Ws2_32", "Advapi32", "Bcrypt", "Userenv")
	end
	local function add_rs_link(str)
		target:add("linkdirs", path.absolute(path.join("../rust/target", str), os.scriptdir()), {
			public = true
		})
		target:add("links", "luisa_compute_ir.dll", {
			public = true
		})
	end
	if is_mode("debug") then
		add_rs_link("debug")
	else
		add_rs_link("release")
	end
end)
after_build(function(target)
	local function copy_dll(str)
		local lib_path = path.absolute(path.join("../rust/target", str), os.scriptdir())
		local bin_dir = target:targetdir()
		if is_plat("windows") then
			os.cp(path.join(lib_path, "luisa_compute_ir.dll"), bin_dir)
		else			
			os.cp(path.join(lib_path, "luisa_compute_ir.so"), bin_dir)
		end
	end
	if is_mode("debug") then
		copy_dll("debug")
	else
		copy_dll("release")
	end
end)
add_headerfiles("../../include/luisa/ir/**.h")
add_files("**.cpp")
target_end()
