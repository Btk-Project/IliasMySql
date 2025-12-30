add_rules("mode.debug", "mode.release", "mode.releasedbg", "mode.coverage")

set_languages("c++20")
add_includedirs("./include")
set_encodings("utf-8")
set_version("0.0.1", {build = "%Y%m%d%H%M"})
set_warnings("allextra")
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")

add_configfiles("include/ilias/sql/global/config.h.in")
set_configdir("include/ilias/sql/global")
set_configvar("API_VERSION", 1)

add_requires("ilias", {version = "0.3.2", configs = {log = true, cpp20 = true}})
add_requires("neko-proto-tools", {version = "dev", configs = {shared = is_config("3rd_kind", "shared"), enable_rapidxml = false, enable_simdjson = false, enable_protocol = false, enable_rapidjson = false, enable_fmt = false, enable_communication = false}})

add_requireconfs("**.ilias", {version = "0.3.2", override = true, configs = {log = true, cpp20 = true}})
add_requireconfs("**.neko-proto-tools", {override = true, version = "dev", configs = {shared = is_config("3rd_kind", "shared"), enable_rapidxml = false, enable_simdjson = false, enable_protocol = false, enable_rapidjson = false, enable_fmt = false, enable_communication = false}})

includes("lua/hidetargets.lua")
set_warnings("allextra")

option("enable_test")
    set_default(false)
    set_showmenu(true)
    set_category("test")
    set_description("enable test")
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

if is_mode("debug") and is_plat("linux") then
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.undefined", true)
end

target("ilias_sql")
    add_options("enable_mysql", "enable_sqlite", "enable_orm_interface", "dynamic_plugin", "enable_postgres")
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