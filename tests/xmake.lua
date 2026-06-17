add_requires("gtest", "cpptrace")

if is_host("linux") then
    add_cxflags("-ftemplate-backtrace-limit=0") -- 0 表示不限制深度
    add_cxflags("-fno-elide-type")              -- 显示完整的类型名称，不简写
end

-- Make all files in the unit directory into targets
for _, file in ipairs(os.files("unit/**/test_*.cpp")) do
    local name = path.basename(file)
    local dir = path.directory(file)
    local conf_path = dir .. "/" .. name .. ".lua"

    -- If this file require a specific configuration, load it, and skip the auto target creation
    if os.exists(conf_path) then 
        print("include " .. conf_path)
        includes(conf_path)
        goto continue
    end
    sql_backend = string.match(dir, "unit/(%w+)")
    if sql_backend == nil then
        sql_backend = string.match(dir, "unit\\(%w+)")
    end
    if sql_backend == nil then
        -- print("skip " .. dir .. "/" .. name)
        goto continue
    end

    if not has_config("enable_" .. sql_backend) then
        -- print("skip " .. dir .. "/" .. name .. " because " .. sql_backend .. " is disabled")
        goto continue
    end

    local group = path.filename(path.directory(file))
    if group == nil or group == "" or group == "." then
        group = "base"
    end

    -- Otherwise, create a target for this file, in most case, it should enough
    target(name .. "_" .. sql_backend)
        set_kind("binary")
        set_default(false)
        add_deps("ilias_sql")
        add_files(file)
        set_group(group)
        local cpp_versions = {stdcxx()}
        for i = 1, #cpp_versions do
            add_tests(cpp_versions[i]:gsub("%+", "p", 2), {group = group, run_timeout = 30000, languages=cpp_versions[i]})
        end
        add_packages("gtest", "cpptrace")
        add_includedirs("$(projectdir)/include")
    target_end()

    ::continue::
end