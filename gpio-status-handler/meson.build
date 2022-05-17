
sdbusplus = dependency(
    'sdbusplus',
    fallback: ['sdbusplus', 'sdbusplus_dep'],
)

gpio_device = dependency('libgpiod')
threads = dependency('threads')

gpio_status_handlerd = executable(
    'gpio-status-handlerd',
    'gpio_status_handler.cpp',
    'gpio_chips.cpp',
    'gpio_lines.cpp',
    'gpio_json_config.cpp',
    'gpio_utils.cpp',
    implicit_include_directories: true,
    dependencies: [sdbusplus,
                   gpio_device,
                   phosphor_logging_dep,
                   threads],
    install_dir: bindir,
    install : true)
