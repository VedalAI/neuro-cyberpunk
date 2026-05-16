set_project("NeuroInteractions")

set_version("0.0.13", { build = "%y%m%d%H" })

set_plat("windows")
set_arch("x64")
set_languages("c++latest")

set_symbols("debug")
set_strip("all")
set_optimize("fastest")
set_runtimes("MD")
add_cxxflags("/GR-")

add_requires("fmt", "glaze", "hopscotch-map", "safetyhook", "semver", "wil")

-- For libneurosdk
includes("deps/xmake.lua")

-- Should really figure out a better way for this
includes("deps/sharedpunk/xmake.lua")

local cp2077_path = os.getenv("CP2077_PATH")

target("NeuroInteractions")
    set_default(true)
    set_kind("shared")
    set_filename("NeuroInteractions.dll")
    set_warnings("more")
    add_files("src/**.cpp", "src/**.rc")
    add_headerfiles("src/**.hpp")
    add_includedirs("src/")
    add_deps("cp2077-shared-data", "libneurosdk", "red4ext.sdk", "redlib")
    add_packages("fmt", "glaze", "hopscotch-map", "safetyhook", "semver", "wil")
    add_syslinks("Version", "User32")
    add_defines("WINVER=0x0601", "WIN32_LEAN_AND_MEAN", "NOMINMAX")
    set_configdir("src")
    add_configfiles("config/ProjectTemplate.hpp.in", { prefixdir = "Config" })
    add_configfiles("config/ProjectMetadata.rc.in", { prefixdir = "Config" })
    set_configvar("NAME", "NeuroInteractions")
    set_configvar("DESC", "Neuro-sama bindings for Cyberpunk 2077")
    set_configvar("AUTHOR_NAME", "alphanin9")
    set_pcxxheader("src/Precompiled.hpp")
    add_cxxflags("/Oi", "/Os")
    set_rundir(path.join(cp2077_path, "bin", "x64"))
    on_package(function(target)
        os.rm("packaging/*")
        os.rm("packaging_pdb/*")

        os.mkdir("packaging/red4ext/plugins/NeuroInteractions")
        os.mkdir("packaging/red4ext/plugins/NeuroInteractions/Scripts")
        os.cp("scripts/*", "packaging/red4ext/plugins/NeuroInteractions/Scripts")

        local target_file = target:targetfile()

        os.cp(target_file, "packaging/red4ext/plugins/NeuroInteractions")
        os.mkdir("packaging_pdb/red4ext/plugins/NeuroInteractions")

        os.cp(
            path.join(
                path.directory(target_file),
                path.basename(target_file) .. ".pdb" -- Evil hack
            ),
            "packaging_pdb/red4ext/plugins/NeuroInteractions"
        )
    end)
    on_install(function(target)
        local target_file = target:targetfile()
        local plugin_folder = path.join(cp2077_path, "red4ext/plugins/NeuroInteractions/")

        -- Note: while symlinks could be nice here,
        -- symlinks seem to work poorly with folders on Windows so we can't just link script dir...
        os.mkdir(plugin_folder)

        os.cp(target_file, plugin_folder)
        os.cp(
            path.join(
                path.directory(target_file),
                path.basename(target_file) .. ".pdb" -- Evil hack #2
            ),
            plugin_folder
        )

        -- Handle script dir if not present
        if not os.exists(path.join(plugin_folder, "Scripts")) then
            -- Note: this actually creates symbolic link to script dir in repo
            -- This is good because we can edit directly in game directory and don't have to copy stuff over
            os.cp(path.absolute("./scripts"), path.join(plugin_folder, "Scripts/"))
        end

        cprint("${bright green}Installed plugin to " .. plugin_folder)
    end)
    on_run(function(target)
        os.run(path.join(cp2077_path, "bin", "x64", "Cyberpunk2077.exe"))
    end)

target("red4ext.sdk")
    set_default(false)
    set_kind("headeronly")
    set_group("deps")
    add_headerfiles("deps/red4ext.sdk/vendor/**.h")
    add_headerfiles("deps/red4ext.sdk/include/**.hpp")
    add_includedirs("deps/red4ext.sdk/vendor/", { public = true })
    add_includedirs("deps/red4ext.sdk/include/", { public = true })

target("redlib")
    set_default(false)
    set_kind("headeronly")
    set_group("deps")
    add_defines("NOMINMAX")
    add_headerfiles("deps/redlib/vendor/**.hpp")
    add_headerfiles("deps/redlib/include/**.hpp")
    add_includedirs("deps/redlib/vendor/", { public = true })
    add_includedirs("deps/redlib/include/", { public = true })

add_rules("plugin.vsxmake.autoupdate")
add_rules("plugin.compile_commands.autoupdate")

set_policy("build.optimization.lto", true)
