add_rules("mode.debug", "mode.release", "mode.releasedbg", "mode.coverage")

set_languages("c++20")
add_includedirs("./include")
set_encodings("utf-8")

add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")

add_requires("ilias", {version = "0.3.2", config = {cpp20 = true, log = true}})

add_packages("ilias")
set_warnings("allextra")

set_languages("c++latest")
option("sql")
    set_default(true)
    set_showmenu(true)
    set_category("module")
    set_description("add sql support")
option_end()

option("sqlite")
    set_default(true)
    set_showmenu(true)
    set_category("module")
    set_description("add sqlite support")
option_end()

if has_config("sql") then
    add_requires("mariadb-connector-c")
    add_packages("mariadb-connector-c")
end

if has_config("sqlite") then
    add_requires("sqlite3")
    add_packages("sqlite3")
end

target("ilias_mysql")
    set_kind("shared")
    add_headerfiles("include/(ilias/**.hpp)")
    add_files("src/**.cpp")
    -- add_headerfiles("(ilias/**.cpp)")
target_end()

includes("./tests")