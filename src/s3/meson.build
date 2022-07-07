# NOTE: https://github.com/mesonbuild/meson/issues/8089
# Most of the lines here are intended to hack around meson cmake plugin which
# does not support cmake subporject dependencies handling.
# All hardcoded paths are horribly wrong and should be removed as soon as meson
# will be fixed to support such kind of dependencies.

cc = meson.get_compiler('c')
libdl = cc.find_library('dl', required: true)

prebuilt_openssl_root = get_option('PREBUILT_OPENSSL_DIR')

cmake = import('cmake')

opt_var = cmake.subproject_options()

curl_cmake_defs = [
  {'BUILD_SHARED_LIBS': 'NO'},
  {'BUILD_CURL_EXE': 'OFF'},
  {'HTTP_ONLY': 'ON'},
  {'BUILD_TESTING': 'OFF'},
]
# ACAP2 SDK has no openssl and zlib
if host_machine.cpu_family() == 'mips'
  curl_cmake_defs += [
    {'ZLIB_LIBRARY': meson.build_root() / 'subprojects/zlib-1.2.11/libz.a'},
    {'ZLIB_INCLUDE_DIR': meson.source_root() / 'subprojects/zlib-1.2.11/'}
  ]
endif

openssl_headers = ['-I' + meson.source_root() / 'subprojects/zlib-1.2.11/']

# Hack: use prebuilt openssl if specified, mainly used for ACAP2 mips build
if prebuilt_openssl_root != ''
  openssl_headers += ['-I' + prebuilt_openssl_root / 'include']
  curl_cmake_defs += [{'OPENSSL_ROOT_DIR': prebuilt_openssl_root}]
endif

# Call CMake with `-DSOME_OTHER_VAR=ON`
opt_var.add_cmake_defines(curl_cmake_defs)
opt_var.append_compile_args('c', openssl_headers)
opt_var.append_compile_args('cpp', openssl_headers)

libcurl_proj = cmake.subproject('libcurl', options: opt_var)
curl_dep = libcurl_proj.dependency('libcurl')
libz_dep = dependency('zlib',
    fallback: ['zlib', 'zlib_dep'], version : '>= 1.2.8')

opt_var = cmake.subproject_options()

opt_var.append_compile_args('c',
  ['-I' + meson.source_root() / 'subprojects/libcurl/include/'] + openssl_headers)
opt_var.append_compile_args('cpp',
  ['-I' + meson.source_root() / 'subprojects/libcurl/include/'] + openssl_headers)

aws_cmake_defs = [
  {'CURL_LIBRARY': meson.build_root() / 'subprojects/libcurl/libcm_libcurl.a'},
  {'CURL_INCLUDE_DIR': meson.source_root() / 'subprojects/libcurl/include/'},
  {'NO_HTTP_CLIENT': 'OFF'},
  {'BUILD_ONLY': 's3;sts;transfer'},
  {'BUILD_SHARED_LIBS': 'OFF'},
  {'S2N_NO_PQ_ASM': 'YES'},
  {'S2N_NO_PQ': 'YES'},
]

# ACAP2 SDK has no openssl and zlib
if host_machine.cpu_family() == 'mips'
  aws_cmake_defs += [
    {'ZLIB_LIBRARY': meson.build_root() / 'subprojects/zlib-1.2.11/libz.a'},
    {'ZLIB_INCLUDE_DIR': meson.source_root() / 'subprojects/zlib-1.2.11/'}
  ]
endif

if prebuilt_openssl_root != ''
  aws_cmake_defs += [{'OPENSSL_ROOT_DIR': prebuilt_openssl_root}]
endif

opt_var.add_cmake_defines(aws_cmake_defs)

# Configure the CMake project
sub_proj = cmake.subproject('aws-sdk-cpp', options: opt_var)
message('CMaket targets:\n - ' + '\n - '.join(sub_proj.target_list()))

aws_libs = [
  's2n',
  'aws-c-auth',
  'aws-c-cal',
  'aws-c-common',
  'aws-c-compression',
  'aws-c-event-stream',
  'aws-c-http',
  'aws-c-io',
  'aws-c-mqtt',
  'aws-c-s3',
  'aws-checksums',
  'aws-cpp-sdk-core',
  'aws-crt-cpp',
  'aws-cpp-sdk-sts',
  'aws-cpp-sdk-s3',
  'aws-cpp-sdk-transfer'
]

# Fetch the dependency object
aws_sdk_deps = []
foreach aws_lib : aws_libs
  aws_sdk_deps += sub_proj.dependency(aws_lib)
endforeach

aws_s3_dep = declare_dependency(
  dependencies: [
    aws_sdk_deps, curl_dep,
    dependency('openssl'),
    meson.get_compiler('cpp').find_library('atomic', static: true)
  ]
)
meson.override_dependency('aws-s3-dep', aws_s3_dep)

