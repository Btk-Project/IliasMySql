add_requires("gtest", "cpptrace")

-- Make all files in the unit directory into targets
for _, file in ipairs(os.files("unit/**/test_*.cpp")) do
    local name = path.basename(file)
    local dir = path.directory(file)
    local conf_path = dir .. "/" .. name .. ".lua"

    -- If this file require a specific configuration, load it, and skip the auto target creation
    if os.exists(conf_path) then 
        includes(conf_path)
        goto continue
    end

    if not has_config("enable_" .. string.sub(name, 6, -1)) then 
        goto continue
    end

    -- Otherwise, create a target for this file, in most case, it should enough
    target(name)
        set_kind("binary")
        set_default(false)
        add_deps("ilias_sql")
        add_files(file)
        add_tests("cpp20", {run_timeout = 10000, languages="c++20"})
        add_packages("gtest", "cpptrace")
        add_includedirs("$(projectdir)/include")
    target_end()

    ::continue::
end