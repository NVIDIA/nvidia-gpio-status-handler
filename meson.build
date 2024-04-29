project('c++ nvidia-gpio-status-handler', 'cpp',
  version : '0.1',
  license : 'MIT',
  meson_version: '>=0.58.0',
  default_options: [
            'cpp_std=c++20'])

# Project related links

project_pretty_name = 'nvidia-gpio-status-handler'
project_url = 'https://gitlab-master.nvidia.com/dgx/bmc/nvidia-gpio-status-handler'
project_issues_url = project_url + '/issues/new'
lang = 'cpp'

#summary('Issues',project_issues_url, section: 'Report Issues')

cxx = meson.get_compiler(lang)
bindir = get_option('prefix') + '/' + get_option('bindir')

## Set global arguments
add_global_arguments('-DSOME_TOKEN=value', language : lang)

# Validate the c++ Standard
if get_option('cpp_std') != 'c++20'
    error('This project requires c++20 support '+ get_option('cpp_std'))
endif

## Fetch all the project options and set the project arguments
debug_log_level = get_option('debug_log')
if debug_log_level != 0
    add_project_arguments('-DDEF_DBG_LEVEL=' + debug_log_level.to_string(), language : lang)
endif
if get_option('sandbox_mode').enabled()
    add_project_arguments('-DSANDBOX_MODE', language : lang)
endif

if get_option('log_elapsed_time').enabled()
    add_project_arguments('-DLOG_ELAPSED_TIME', language : lang)
endif


# Get the options and enable the respective features
## create a MAP of  "options : feature_flag"

feature_map = {
}

# Get the options status and build a project summary to show which flags are
# being enabled during the configuration time.

foreach option_key,option_value : feature_map
  if(get_option(option_key).enabled())
      summary(option_key,option_value, section : 'Enabled Features')
      add_global_arguments(option_value,language : lang)
  endif
endforeach

systemd_dep = dependency('systemd')

sdbusplus_dep = dependency('sdbusplus', required: false)
sdbusplus_proj = dependency('', required: false)
if not sdbusplus_dep.found() or sdbusplus_dep.type_name() == 'internal'
    sdbusplus_proj = subproject('sdbusplus')
endif
if not sdbusplus_dep.found()
    sdbusplus_dep = sdbusplus_proj.get_variable('sdbusplus_dep')
endif

if sdbusplus_proj.found()
    sdbusplusplus_prog = sdbusplus_proj.get_variable('sdbusplusplus_prog')
    sdbuspp_gen_meson_prog = sdbusplus_proj.get_variable(
        'sdbuspp_gen_meson_prog')
else
    sdbusplusplus_prog = find_program('sdbus++', native: true)
    sdbuspp_gen_meson_prog = find_program('sdbus++-gen-meson', native: true)
endif


phosphor_logging_dep = dependency(
    'phosphor-logging',
    fallback: ['phosphor-logging', 'phosphor_logging_dep'],
)

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
