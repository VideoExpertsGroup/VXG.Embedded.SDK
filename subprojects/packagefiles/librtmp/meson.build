project('librtmp', 'c',
    version : '1.0',
    meson_version : '>= 0.33.0')

openssl_dep = dependency('openssl',
    fallback: ['openssl', 'openssl_dep'], version : '>= 1.0.0')
libz_dep = dependency('zlib',
    fallback: ['zlib', 'zlib_dep'], version : '>= 1.2.8')

add_project_arguments(['-DCRYPTO', '-DUSE_OPENSSL'], language: 'c')

librtmp_headers = [
  'librtmp/amf.h',
  'librtmp/log.h',
  'librtmp/rtmp.h',
  'librtmp/rtmp_sys.h',
]

install_headers(librtmp_headers, subdir : 'librtmp')

librtmp_sources = [
  'librtmp/amf.c',
  'librtmp/hashswf.c',
  'librtmp/log.c',
  'librtmp/parseurl.c',
  'librtmp/rtmp.c'
]

rtmp_lib = library('rtmp',
  librtmp_sources,
  dependencies : [openssl_dep, libz_dep],
  include_directories: include_directories('.'),
  install : true,
)

librtmp_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: rtmp_lib
)

pkgconf = configuration_data()
pkgconf.set('prefix', join_paths(get_option('prefix')))
pkgconf.set('exec_prefix', '${prefix}')
pkgconf.set('libdir', '${prefix}/@0@'.format(get_option('libdir')))
pkgconf.set('includedir', '${prefix}/@0@'.format(get_option('includedir')))
pkgconf.set('VERSION', meson.project_version())
pkgconf.set('CRYPTO_REQ', 'libssl,libcrypto')

configure_file(
    input : 'librtmp/librtmp.pc.in',
    output: 'librtmp.pc',
    configuration : pkgconf,
    install_dir : join_paths(get_option('libdir'), 'pkgconfig'))