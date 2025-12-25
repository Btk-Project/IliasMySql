-- ORM Interface 测试的特殊配置
-- 这个文件会被 tests/xmake.lua 自动包含

target("test_orm_interface_mysql")
    set_kind("binary")
    set_default(false)
    add_deps("ilias_sql")
    add_files("test_orm_interface.cpp")
    add_packages("gtest", "cpptrace")
    add_includedirs("$(projectdir)/include")
    
    -- 添加测试注册
    add_tests("cpp20")
    
    -- 启用覆盖率检测 (仅在 Linux Debug 模式下)
    if is_plat("linux") and is_mode("debug") then
        add_cxflags("--coverage", "-fprofile-arcs", "-ftest-coverage")
        add_ldflags("--coverage", "-lgcov")
        add_links("gcov")
        
        -- 设置覆盖率输出目录
        set_objectdir("$(builddir)/.objs/coverage/$(target)")
        
        -- 确保覆盖率数据能正确生成
        add_cxflags("-O0", "-g")  -- 禁用优化，启用调试信息
    end
    
    -- 测试前后钩子
    before_run(function (target)
        print("=== Starting ORM Interface Tests ===")
        print("Target: " .. target:name())
        print("Mode: " .. get_config("mode"))
        print("Platform: " .. get_config("plat"))
        
        -- 检查数据库连接
        local mysql_available = os.iorun("mysql --version 2>/dev/null")
        if mysql_available then
            print("MySQL client detected")
        else
            print("Warning: MySQL client not found, tests may fail")
        end
    end)
    
    after_run(function (target)
        print("=== ORM Interface Tests Completed ===")
        
        -- 生成覆盖率报告 (如果启用)
        if is_plat("linux") and is_mode("debug") then
            print("Generating coverage report...")
            
            local target_name = target:name()
            local build_dir = target:targetdir()
            local obj_dir = target:objectdir()
            local project_dir = os.projectdir()
            local coverage_dir = path.join(build_dir, "coverage")
            local report_dir = path.join(build_dir, "coverage_report")
            
            -- 创建覆盖率报告目录
            os.mkdir(coverage_dir)
            os.mkdir(report_dir)
            
            -- 查找所有 .gcda 和 .gcno 文件
            local gcda_files = os.files(path.join(obj_dir, "**.gcda"))
            local gcno_files = os.files(path.join(obj_dir, "**.gcno"))
            
            if #gcda_files > 0 and #gcno_files > 0 then
                print("Found coverage data files:")
                print("  GCDA files: " .. #gcda_files)
                print("  GCNO files: " .. #gcno_files)
                
                -- 生成 LCOV 报告
                local lcov_info = path.join(coverage_dir, "coverage.info")
                local lcov_cmd = string.format(
                    "lcov --capture --directory %s --output-file %s --base-directory %s",
                    obj_dir, lcov_info, project_dir
                )
                
                print("Running: " .. lcov_cmd)
                local lcov_ok, lcov_err = os.iorun(lcov_cmd)
                
                if lcov_ok and os.exists(lcov_info) then
                    -- 过滤系统头文件和测试文件
                    local filtered_info = path.join(coverage_dir, "coverage_filtered.info")
                    local filter_cmd = string.format(
                        "lcov --remove %s '/usr/*' '*/tests/*' '*/.xmake/packages/*' --output-file %s --ignore-errors unused",
                        lcov_info, filtered_info
                    )
                    
                    print("Running: " .. filter_cmd)
                    local filter_ok, filter_err = os.iorun(filter_cmd)
                    
                    if filter_ok and os.exists(filtered_info) then
                        -- 生成 HTML 报告
                        local html_dir = path.join(report_dir, "html")
                        local genhtml_cmd = string.format(
                            "genhtml %s --output-directory %s --title '%s Coverage Report' --show-details --legend",
                            filtered_info, html_dir, target_name
                        )
                        
                        print("Running: " .. genhtml_cmd)
                        local html_ok, html_err = os.iorun(genhtml_cmd)
                        
                        if html_ok then
                            print("Coverage report generated successfully!")
                            print("HTML Report: " .. path.join(html_dir, "index.html"))
                            print("LCOV Info: " .. filtered_info)
                            
                            -- 显示覆盖率摘要
                            local summary_cmd = string.format("lcov --summary %s", filtered_info)
                            print("Coverage Summary:")
                            os.exec(summary_cmd)
                        else
                            print("Warning: Failed to generate HTML report: " .. (html_err or "unknown error"))
                        end
                    else
                        print("Warning: Failed to filter coverage data: " .. (filter_err or "unknown error"))
                    end
                else
                    print("Warning: Failed to capture coverage data: " .. (lcov_err or "unknown error"))
                    print("Make sure lcov is installed: sudo apt-get install lcov")
                end
            else
                print("Warning: No coverage data files found")
                print("Make sure the test was compiled and run with --coverage flags")
                print("Object directory: " .. obj_dir)
            end
        end
    end)
target_end()