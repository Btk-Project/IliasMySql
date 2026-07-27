add_rules("mode.debug", "mode.release", "mode.releasedbg", "mode.coverage", "mode.asan")

set_version("0.1.0", {build = "%Y%m%d%H%M"})
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")
set_warnings("allextra")
set_encodings("utf-8")
set_policy("package.cmake_generator.ninja", true)
includes("lua/hidetargets.lua")

option("stdc",   {showmenu = true, default = 23, values = {23}})
option("stdcxx", {showmenu = true, default = 20, values = {26, 23, 20}})
function stdc()   return "c"   .. tostring(get_config("stdc"))   end
function stdcxx() return "c++" .. tostring(get_config("stdcxx")) end
set_languages(stdc(), stdcxx())

add_configfiles("include/ilias/sql/global/config.h.in")
set_configdir("include/ilias/sql/global")
set_configvar("API_VERSION", 1)

option("3rd_custom",   {showmenu = true, type = "boolean", default = false})

add_requires("ilias", "neko-proto-tools")

if has_config("3rd_custom") then
    add_requireconfs("**ilias", {version = "x.x.x", override = true, configs = {log = true, shared = is_kind("shared"), stdcxx = tonumber(get_config("stdcxx")) }})
    add_requireconfs("**neko-proto-tools", {override = true, version = "x.x.x", configs = {shared = is_kind("shared"), stdcxx = tonumber(get_config("stdcxx")), enable_rapidxml = false, enable_simdjson = false, enable_protocol = false, enable_rapidjson = false, enable_fmt = false, enable_communication = false, enable_jsonrpc = false}})
end

option("enable_test")
    set_default(false)
    set_showmenu(true)
    set_category("test")
    set_description("enable test")
option_end()

option("memcheck")
    set_default(false)
    set_showmenu(true)
    set_category("test")
    set_description("run tests through valgrind memcheck on Linux")
option_end()

option("enable_mysql")
    set_default(true)
    set_showmenu(true)
    set_category("module")
    set_description("add mysql support, need mariadb-connector-c")
    set_configvar("ENABLE_MYSQL_PLUGINS", true)
option_end()

option("enable_sqlite")
    set_default("sqlite")
    set_showmenu(true)
    set_values("disable", "sqlite", "sqlcipher")
    set_category("module")
    set_description("add sqlite support, need sqlite3")
    after_check(function (option)
        if option:value() == "sqlite" then
            option:set("configvar", "ENABLE_SQLITE_PLUGINS", true)
        end
        if option:value() == "sqlcipher" then
            option:set("configvar", "ENABLE_SQLITE_PLUGINS", true)
            option:set("configvar", "ENABLE_SQLCIPHER_PLUGINS", true)
            option:add("defines", "SQLITE_HAS_CODEC")
        end
    end)
option_end()

option("enable_postgres")
    set_default(false)
    set_showmenu(true)
    set_category("module")
    set_description("add postgres support, need libpq")
    set_configvar("ENABLE_POSTGRES_PLUGINS", true)
option_end()

option("enable_orm_interface")
    set_default(true)
    set_showmenu(true)
    set_category("module")
    set_description("enable orm interface")
    set_configvar("ENABLE_ORM_INTERFACE", true)
option_end()

option("dynamic_plugin")
    set_default(false)
    set_showmenu(true)
    set_category("module")
    set_description("Build dynamic plugins using this library")
    set_configvar("BUILD_AS_DYNAMIC_PLUGIN", true)
option_end()

if has_config("enable_mysql") then
    add_requires("mariadb-connector-c")
end

if has_config("enable_sqlite") then
    if get_config("enable_sqlite") == "sqlite" then
        add_requires("sqlite3")
    elseif get_config("enable_sqlite") == "sqlcipher" then
        add_requires("sqlcipher")
    end
end

if has_config("enable_postgres") then
    add_requires("libpq")
end

if is_plat("windows") then 
    add_cxxflags("/bigobj", "/Zc:preprocessor")
end

if is_mode("debug") and is_plat("linux") and not has_config("memcheck") then
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.undefined", true)
end

if is_mode("asan") and is_plat("linux") then
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.undefined", true)
    set_policy("build.sanitizer.leak", true)
end

target("ilias_sql")
    add_options("enable_mysql", "enable_sqlite", "enable_orm_interface", "dynamic_plugin", "enable_postgres")
    add_includedirs("./include")
    if has_config("dynamic_plugin") then
        set_kind("shared")
        add_headerfiles("include/(ilias/sql/interfaces.hpp)")
        add_headerfiles("include/(ilias/sql/sql_plugin.hpp)")
        add_headerfiles("include/(ilias/sql/global/**.h)")
        add_headerfiles("include/(ilias/sql/global/**.hpp)")
        add_files("src/sql/**.cpp")
        if has_config("enable_sqlite") then
            add_headerfiles("include/(ilias/sqlite/**.hpp)")
            add_files("src/sqlite/**.cpp")
        end
    else
        add_headerfiles("include/(ilias/sql/**.hpp)")
        add_headerfiles("include/(ilias/sql/**.h)")
        add_files("src/sql/**.cpp")
        if has_config("enable_mysql") then
            add_headerfiles("include/(ilias/mysql/**.hpp)")
            add_files("src/mysql/**.cpp")
        end
        if has_config("enable_sqlite") then
            add_headerfiles("include/(ilias/sqlite/**.hpp)")
            add_files("src/sqlite/**.cpp")
        end
        if has_config("enable_postgres") then
            add_headerfiles("include/(ilias/postgres/**.hpp)")
            add_files("src/postgres/**.cpp")
        end
        if has_config("enable_orm_interface") then
            add_headerfiles("include/(ilias/sql_orm/**.hpp)")
            add_files("src/sql_orm/**.cpp")
        end
        if is_kind("shared") then
            set_kind("shared")
            add_defines("ILIAS_SQL_LIBRARY")
        else
            set_kind("static")
            set_configvar("ILIAS_SQL_STATIC", true)
        end
    end
    on_load(function (target)
        import("lua.auto", {rootdir = os.projectdir()})
        auto().auto_add_packages(target)
    end)
target_end()

if has_config("enable_test") then
    includes("./tests")
end
