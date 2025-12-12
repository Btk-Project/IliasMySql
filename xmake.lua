add_rules("mode.debug", "mode.release", "mode.releasedbg", "mode.coverage")

set_languages("c++20")
add_includedirs("./include")
set_encodings("utf-8")
set_version("0.0.1", {build = "%Y%m%d%H%M"})
set_warnings("allextra")
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")

add_configfiles("include/ilias/sql/global/config.h.in")
set_configdir("include/ilias/sql/global")

add_requires("ilias", {version = "0.3.2", configs = {log = true, cpp20 = true}})
add_requires("neko-proto-tools", {version = "dev", configs = {shared = is_config("3rd_kind", "shared"), enable_rapidxml = false, enable_simdjson = false, enable_protocol = false, enable_rapidjson = false, enable_fmt = false, enable_communication = false}})

add_requireconfs("**.ilias", {version = "0.3.2", override = true, configs = {log = true, cpp20 = true}})
add_requireconfs("**.neko-proto-tools", {override = true, version = "dev", configs = {shared = is_config("3rd_kind", "shared"), enable_rapidxml = false, enable_simdjson = false, enable_protocol = false, enable_rapidjson = false, enable_fmt = false, enable_communication = false}})

includes("lua/hidetargets.lua")
set_warnings("allextra")

option("enable_mysql")
    set_default(true)
    set_showmenu(true)
    set_category("module")
    set_description("add mysql support, need mariadb-connector-c")
    set_configvar("ENABLE_MYSQL_PLUGINS", true)
option_end()

option("enable_sqlite")
    set_default(true)
    set_showmenu(true)
    set_category("module")
    set_description("add sqlite support, need sqlite3")
    set_configvar("ENABLE_SQLITE_PLUGINS", true)
option_end()

if has_config("enable_mysql") then
    add_requires("mariadb-connector-c")
end

if has_config("enable_sqlite") then
    add_requires("sqlite3")
end

if is_plat("windows") then 
    add_cxxflags("/bigobj", "/Zc:preprocessor")
end

if is_mode("debug") then
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.undefined", true)
end

target("ilias_sql")
    add_headerfiles("include/(ilias/sql/**.hpp)")
    add_files("src/sql/**.cpp")
    add_options("enable_mysql", "enable_sqlite")
    if has_config("enable_mysql") then
        add_headerfiles("include/(ilias/mysql/**.hpp)")
        add_files("src/mysql/**.cpp")
    end
    if has_config("enable_sqlite") then
        add_headerfiles("include/(ilias/sqlite/**.hpp)")
        add_files("src/sqlite/**.cpp")
    end
    if is_kind("shared") then
        set_kind("shared")
        add_defines("ILIAS_SQL_LIBRARY")
    else
        set_kind("static")
        set_configvar("ILIAS_SQL_STATIC", true)
    end
    on_load(function (target)
        import("lua.auto", {rootdir = os.projectdir()})
        auto().auto_add_packages(target)
    end)
target_end()

includes("./tests")