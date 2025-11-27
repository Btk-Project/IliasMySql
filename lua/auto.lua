local autofunc = autofunc or {}

import("core.project.project")

-- private

function _Camel(str)
    return str:sub(1, 1):upper() .. str:sub(2)
end

-- public

function autofunc.target_autoclean(target)
    os.tryrm(target:targetdir() .. "/" .. target:basename() .. ".exp")
    os.tryrm(target:targetdir() .. "/" .. target:basename() .. ".ilk")
    os.tryrm(target:targetdir() .. "/compile." .. target:basename() .. ".pdb")
end

function autofunc.auto_add_packages(target)
    if has_config("enable_mysql") then
        target:add("packages", "mariadb-connector-c", {public = true})
    end

    if has_config("enable_sqlite") then
        target:add("packages", "sqlite3", {public = true})
    end

    target:add("packages", "ilias", {public = true})
    target:add("packages", "neko-proto-tools", {public = true})
end

function main()
    return autofunc
end