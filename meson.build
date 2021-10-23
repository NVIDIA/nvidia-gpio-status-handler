project('c++ nvidia-oobaml', 'cpp',
  version : '0.1',
  license : 'MIT',
  default_options: [
            'cpp_std=c++11'])


# Project related links

project_pretty_name = 'nvidia-oobaml'
project_url = 'https://gitlab-master.nvidia.com/dgx/nvidia-oobaml'
project_issues_url = project_url + '/issues/new'
lang = 'cpp'

#summary('Issues',project_issues_url, section: 'Report Issues')

cxx = meson.get_compiler(lang)

## Set global arguments
add_global_arguments('-DSOME_TOKEN=value', language : lang)

# Validate the c++ Standard
if get_option('cpp_std') != 'c++11'
    error('This project requires c++11 support'+ get_option('cpp_std'))
endif

## Fetch all the project options and set the project arguments
debug_log_level = get_option('debug_log')
if debug_log_level != 0
    add_project_arguments('-DDEF_DBG_LEVEL=debug_log_level', language : lang)
endif
if get_option('sandbox_mode').enabled()
    add_project_arguments('-DSANDBOX_MODE', language : lang)
endif

# Add all the dependencies
aml_dependencies = []

aml_dependencies += dependency('glib-2.0')
aml_dependencies += dependency('threads')

if cxx.has_header('nlohmann/json.hpp')
    aml_dependencies += declare_dependency()
endif

librtdep = dependency('librt', required: false)
if not librtdep.found()
    librtdep = cxx.find_library('rt')
endif

aml_dependencies += librtdep

# Add Include Directory
inc = include_directories('include')

# subdir for meson project
subdir('include')
subdir('src')
#subdir('test')

# Pkg-config
pkg_mod = import('pkgconfig')
pkg_mod.generate(libraries : amllib,
                 version : '0.1',
                 name : 'oobaml',
                 filebase : 'oobaml',
                 description : 'nvidia-oobaml')