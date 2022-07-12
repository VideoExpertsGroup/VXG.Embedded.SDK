project('vxgcloudagent', ['c', 'cpp'],   version : '1.3.2',
  meson_version : '>= 0.56.0',
  default_options : [ 'warning_level=1',
                      'buildtype=debug',
                      'auto_features=disabled',
                      'default_library=static',
                      'cpp_std=c++11', 'cpp_link_args=-static-libstdc++' ])

if not meson.is_subproject()
  # Install the git pre-commit hook if we are in standalone build
  git_hook = run_command(join_paths(meson.source_root(),
                                    'scripts/pre-commit-hook'), 'check-installed')
  if git_hook.returncode() == 0
    message('Git pre-commit hook installed')
  else
    message('Installing Git pre-commit hook')
    run_command(join_paths(meson.source_root(),
      'scripts/pre-commit-hook'), 'install')
  endif
endif

vxgcloudagent_version = meson.project_version()

need_native = true
if meson.is_cross_build() == true
  need_native = false
endif

have_cc = add_languages('c', required : true, native: need_native)
have_cxx = add_languages('cpp', required : true, native: need_native)
cc = meson.get_compiler('c', native: need_native)
cxx = meson.get_compiler('cpp', native: need_native)

# config.h generation, dlfcn.h and asinh() just as an example
cdata = configuration_data()
check_headers = [
    ['HAVE_DLFCN_H', 'dlfcn.h']
]

foreach h : check_headers
  if cc.has_header(h.get(1))
    cdata.set(h.get(0), 1)
  else
    cdata.set(h.get(0), 0)
  endif
endforeach

bt_lib = ''
if cc.has_header('execinfo.h')
  message('C compiler has execinfo.h')
elif get_option('backtrace_lib') != ''
  bt_lib = get_option('backtrace_lib')
  message('C compiler has no execinfo.h, external implementation ' + bt_lib +
      ' will be used')
else
  message('backtrace() will not be used')
  cdata.set('LOGURU_STACKTRACES', 0)
endif

check_functions = [
    ['HAVE_ASINH', 'asinh', '#include<math.h>'],
]

libm = cc.find_library('m', required : false)

foreach f : check_functions
  if cc.has_function(f.get(1), prefix : f.get(2), dependencies : libm)
    cdata.set(f.get(0), 1)
  endif
endforeach

# Corelib version
cdata.set_quoted('VXGCLOUDAGENT_LIB_VERSION', vxgcloudagent_version)

# Access token fields default values
cdata.set_quoted('DEFAULT_CLOUD_TOKEN_API', get_option('DEFAULT_CLOUD_TOKEN_API'))
cdata.set('DEFAULT_CLOUD_TOKEN_API_P', get_option('DEFAULT_CLOUD_TOKEN_API_P'))
cdata.set('DEFAULT_CLOUD_TOKEN_API_SP', get_option('DEFAULT_CLOUD_TOKEN_API_SP'))
cdata.set_quoted('DEFAULT_CLOUD_TOKEN_CAM', get_option('DEFAULT_CLOUD_TOKEN_CAM'))
cdata.set('DEFAULT_CLOUD_TOKEN_CAM_P', get_option('DEFAULT_CLOUD_TOKEN_CAM_P'))
cdata.set('DEFAULT_CLOUD_TOKEN_CAM_SP', get_option('DEFAULT_CLOUD_TOKEN_CAM_SP'))

# SSL related settings
cdata.set_quoted('OPENSSL_CIPHERS_LIST', get_option('OPENSSL_CIPHERS_LIST'))
if get_option('SSL_ALLOW_INSECURE_CERTS')
  cdata.set('SSL_ALLOW_INSECURE_CERTS', 1)
else
  cdata.set('SSL_ALLOW_INSECURE_CERTS', 0)
endif

props_file = get_option('props_file_path')
message('Properties file option set to ' + props_file)
cdata.set_quoted('PROPS_FILE_PATH', props_file)

if get_option('librtmp').enabled()
  cdata.set('RTMP_SOCKS4', 1)
else
  cdata.set('RTMP_SOCKS4', 0)
endif

cdata.set('HAVE_CPP_LIB_FILESYSTEM', cc.find_library('stdc++fs', required : false).found())

if get_option('SSL_CERTS_BUNDLE_FILE') != ''
  cdata.set_quoted('SSL_CERTS_BUNDLE_FILE', get_option('SSL_CERTS_BUNDLE_FILE'))
endif

configure_file(output : 'vxgcloudagent-config.h', configuration : cdata)

# Warnings
warning_flags = [
  '-Wmissing-declarations',
  '-Wredundant-decls',
  '-Wwrite-strings',
  '-Winit-self',
  '-Wmissing-include-dirs',
  '-Wno-multichar',
  '-Wvla',
  '-Wpointer-arith',
  # FIXME:
  '-Wno-reorder',
  '-Wno-write-strings',
  '-Wno-unused-function',
  '-Wno-missing-include-dirs',
  '-Wno-unused-function',
  '-Wno-psabi',
  '-Wno-missing-declarations',
  '-Wno-unused-function'
]

warning_c_flags = [
  '-Wmissing-prototypes',
  '-Wdeclaration-after-statement',
  '-Wold-style-definition',
  '-Waggregate-return',
]

foreach extra_arg : warning_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
  if have_cxx and cxx.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'cpp')
  endif
endforeach

foreach extra_arg : warning_c_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
endforeach

# Disables usage of g++5.0+ locale facets in the date library
# We use date.h header so we need to define this flag globally
if cxx.version().version_compare('< 5')
  add_project_arguments(['-DONLY_C_LOCALE=1'], language: 'cpp')
endif

core_srcs = [
    'src/agent-proto/command/command.cc',
    'src/agent-proto/command-handler.cc',
    'src/agent/manager.cc',
    'src/agent/event-manager.cc',

    'src/utils/packbits.c',
    'src/utils/loguru.cc',
    'src/utils/logging.cc',
    'src/utils/utils.cc',
    'src/utils/base64.cc',
    'src/utils/properties.cc',

    'src/net/websockets.cc',
    'src/net/http.cc',

    'src/streamer/multifrag_sink.cc',
    'src/streamer/stats.c'
]

subdir('src')

websockets = dependency('libwebsockets',
    fallback: ['libwebsockets', 'libwebsockets_dep'], required: true)

if get_option('LWS_WITH_SSHD') and websockets.type_name() == 'internal'
  core_srcs += [
    'src/net/sshd.cc',
    'src/net/lws-sshd.c'
  ]
endif

spdlog = dependency('spdlog',
    fallback: ['spdlog', 'spdlog_dep'], required: true)
threads = dependency('threads', required: true)
json = dependency('nlohmann_json',
    fallback: ['nlohmann_json', 'nlohmann_json_dep'], required: true)

date_opts = []
if cc.version().version_compare('< 5')
  date_opts += ['ONLY_C_LOCALE=enabled']
endif

date = dependency('date',
    fallback: ['date', 'date_dep'], required: true, default_options: date_opts)
dl = cc.find_library('dl', required : false)
cpp11fs = cc.find_library('stdc++fs', required : false)
args = dependency('args')
tzmap = dependency('tzmap')
zlib = dependency('zlib', fallback: ['zlib', 'zlib_dep'])
deps = [ websockets, spdlog, threads, json, date, args, zlib, tzmap, cpp11fs, dl ]

if get_option('ffmpeg').enabled()
    core_srcs += [
        'src/streamer/ffmpeg_sink.cc',
        'src/streamer/ffmpeg_source.cc',
        'src/streamer/ffmpeg_common.cc'
    ]

    ffmpeg = [
        dependency('libavformat',
            fallback : ['ffmpeg', 'libavformat_dep'], static: true),
        dependency('libavcodec',
            fallback : ['ffmpeg', 'libavcodec_dep'],static: true),
        dependency('libavutil',
            fallback : ['ffmpeg', 'libavutil_dep'],static: true),
    ]

    deps += ffmpeg
endif

if get_option('librtmp').enabled()
  librtmp = dependency('librtmp',
      fallback: ['librtmp', 'librtmp_dep'], required: true)
  deps += librtmp
endif

if meson.is_cross_build() and bt_lib != ''
  deps += dependency(bt_lib)
  message('Using backtrace lib @0@'.format(bt_lib))
endif

if get_option('AWS_S3_SUPPORT').enabled()
  message('Amazon CPP SDK Enabled')
  subdir('src/s3')

  core_srcs += [ 'src/s3/s3-storage.cc' ]

  deps += dependency('aws-s3-dep')
endif

vxgcloudagent_includes = include_directories(['.', 'src/'])
vxgcloudagent = library('vxgcloudagent',
    core_srcs,
    dependencies: deps,
    include_directories: [vxgcloudagent_includes],
    install: true)
vxgcloudagent_dep = declare_dependency(
    include_directories: [vxgcloudagent_includes],
    link_with: vxgcloudagent,
    dependencies: deps)

if not meson.is_subproject()
  pkg = import('pkgconfig')
  pkg.generate(vxgcloudagent)
endif

#
# tests
#
if meson.is_cross_build() == false and not meson.is_subproject()
  subdir('src/tests')
endif

if not meson.is_subproject()
  args = dependency('args')
# //! [meson-example-target]
  executable('cloud-agent-minimal',
      sources: [ 'examples/app/src/cloud-agent-minimal.cc' ],
      dependencies : [vxgcloudagent_dep, args],
      install: true
  )
# //! [meson-example-target]
  executable('cloud-agent',
      sources: [ 'examples/app/src/cloud-agent.cc' ],
      dependencies : [vxgcloudagent_dep, args],
      install: true
  )

  if get_option('streamer_test').enabled() and get_option('ffmpeg').enabled()
      streamer_test_src = ['examples/streaming/ffmpeg.cc']
      executable('ffmpeg_restreamer', sources: streamer_test_src,
          dependencies : [ vxgcloudagent_dep ],
          include_directories: include_directories('src/'),
      )
      streamer_test_src = ['examples/streaming/ffmpeg_multipart_record.cc']
      executable('ffmpeg_record', sources: streamer_test_src,
          dependencies : [ vxgcloudagent_dep ],
          include_directories: include_directories('src/'),
      )
      streamer_test_src = ['examples/streaming/ffmpeg_rtmp2fake.cc']
      executable('rtmp2http', sources: streamer_test_src,
          dependencies : [ vxgcloudagent_dep, ffmpeg ],
          include_directories: include_directories('src/'),
      )

      streamer_test_src = ['examples/streaming/rtsp2rtmp.cc']
      executable('rtsp2rtmp', sources: streamer_test_src,
          dependencies : [ vxgcloudagent_dep, ffmpeg ],
          include_directories: include_directories('src/'),
      )

      remux_test_src = ['examples/streaming/remux.cc']
      executable('ffmpeg_remux', sources: remux_test_src,
          dependencies : [ vxgcloudagent_dep, ffmpeg ],
          include_directories: include_directories('src/'),
      )
  endif
endif

if get_option('documentation')
  subdir('doc/')
endif
