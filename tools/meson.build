selftest_tool_sources = ['selftest_tool.cpp']
selftest_tool_deps = declare_dependency(link_with : [amllib, selftestlib],
  include_directories : inc)

selftest_tool = executable('selftest_tool',
    selftest_tool_sources,
    include_directories: inc,
    dependencies: [
        selftest_tool_deps,
        phosphor_logging_dep,
    ],
    install: true,
)
