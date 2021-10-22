aml_sources = ['config_parser.cpp',
                'dat_traverse.cpp',
                'data_accessor.cpp',
                'event_detection.cpp',
                'event_handler.cpp',
                'logger.cpp',
                'message_composer.cpp',
                'message_dispatcher.cpp',
                'oobaml_api.cpp']

amllib = shared_library('nvidia-oobaml',
                        aml_sources,
                        include_directories : inc,
                        dependencies : aml_dependencies,
                        version : '0.0.0',
                        install : true,
                        install_dir : get_option('libdir'))