project('date', [],
    meson_version : '>=0.55.3')

internal_inc = include_directories('include')
date_lib = library('date', ['src/tz.cpp'],
    install: true,
    include_directories: internal_inc,
    cpp_args: ['-DHAS_REMOTE_API=0', '-DUSE_OS_TZDB=1']
)

date_dep = declare_dependency(
    include_directories: internal_inc,
    link_with: date_lib
)
