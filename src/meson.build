amllib_sources = [
    'check_accessor.cpp',
    'config_parser.cpp',
    'dat_traverse.cpp',
    'data_accessor.cpp',
    'dbus_accessor.cpp',
    'device_id.cpp',
    'device_util.cpp',
    'event_detection.cpp',
    'event_handler.cpp',
    'event_info.cpp',
    'json_proc.cpp',
    'log.cpp',
    'message_composer.cpp',
    'message_dispatcher.cpp',
    'oobaml_api.cpp',
    'property_accessor.cpp',
    'util.cpp']

amllib = shared_library('oobaml',
                        amllib_sources,
                        include_directories : inc,
                        dependencies : aml_dependencies,
                        version : '0.0.0',
                        install : true,
                        install_dir : get_option('libdir'))

selftestlib_sources = ['selftest.cpp']
selftestlib = shared_library('selftest',
                        selftestlib_sources,
                        include_directories : inc,
                        dependencies : [
                                        declare_dependency(link_with : amllib),
                                        sdbusplus_dep,
                                        phosphor_logging_dep
                                      ],
                        version : '0.0.0',
                        install : true,
                        install_dir : get_option('libdir'))

oobamld_sources = ['aml_main.cpp']
oobamld_deps = declare_dependency(link_with : [amllib, selftestlib],
  include_directories : inc)

oobamld = executable(
    'oobamld',
    oobamld_sources,
    include_directories: inc,
    dependencies: [
        oobamld_deps,
        sdbusplus_dep,
        sdeventplus_dep,
        phosphor_logging_dep,
        meson.get_compiler('cpp').find_library('rt')
    ],
    install: true,
)
