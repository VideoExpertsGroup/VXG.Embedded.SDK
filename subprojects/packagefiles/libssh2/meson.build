project('libssh2', 'c', version: '1.9.0',
    meson_version : '>=0.55.3')

libssh2_sources = [
    'src/misc.c',
    'src/scp.c',
    'src/crypt.c',
    'src/hostkey.c',
    'src/version.c',
    'src/wincng.c',
    'src/blowfish.c',
    'src/mac.c',
    'src/os400qc3.c',
    'src/transport.c',
    'src/agent.c',
    'src/pem.c',
    'src/kex.c',
    'src/sftp.c',
    'src/knownhost.c',
    'src/publickey.c',
    'src/channel.c',
    'src/global.c',
    'src/packet.c',
    'src/bcrypt_pbkdf.c',
    'src/userauth.c',
    'src/keepalive.c',
    'src/comp.c',
    'src/session.c'
]

libssh2_deps = []

c_compiler = meson.get_compiler('c')

with_openssl = false
with_embedtls = false
with_gcrypt = false
extra_flags = []
if get_option('crypto_backend') == 'openssl'
    with_openssl = true
    libssh2_sources += ['src/openssl.c']

    openssl_dep = dependency('openssl', fallback: ['openssl', 'openssl_dep'])
    libz_dep = dependency('zlib',
        fallback: ['zlib', 'zlib_dep'], version : '>= 1.2.8')
    libssh2_deps += openssl_dep

    extra_flags += ['-DLIBSSH2_OPENSSL']
elif get_option('crypto_backend') == 'embedtls'
    with_embedtls = true
    libssh2_sources += ['src/mbedtls.c']

    # todo: not tested
    embedtls_dep = dependency('embedtls')

    libssh2_deps += embedtls_dep
    extra_flags += ['-DLIBSSH2_EMBEDTLS']
elif get_option('crypto_backend') == 'gcrypt'
    with_gcrypt = true
    libssh2_sources += ['src/libgcrypt.c']

    # todo: not tested
    gcrypt_dep = dependency('gcrypt')

    libssh2_deps += gcrypt_dep
    extra_flags += ['-DLIBSSH2_GCRYPT']
else
    error('No crypto backend specified')
endif

libssh2_have_unistd_h = c_compiler.has_header('unistd.h')
libssh2_have_fcntl_h = c_compiler.has_header('fcntl.h')
libssh2_have_inttypes_h = c_compiler.has_header('inttypes.h')
libssh2_have_stdlib_h = c_compiler.has_header('stdlib.h')
libssh2_have_sys_select_h = c_compiler.has_header('sys/select.h')
libssh2_have_sys_uio_h = c_compiler.has_header('sys/uio.h')
libssh2_have_sys_socket_h = c_compiler.has_header('sys/socket.h')
libssh2_have_sys_ioctl_h = c_compiler.has_header('sys/ioctl.h')
libssh2_have_sys_time_h = c_compiler.has_header('sys/time.h')
libssh2_have_sys_un_h = c_compiler.has_header('sys/un.h')
libssh2_have_windows_h = c_compiler.has_header('windows.h')
libssh2_have_ws2tcpip_h = c_compiler.has_header('ws2tcpip.h')
libssh2_have_winsock2_h = c_compiler.has_header('winsock2.h')
libssh2_have_ntdef_h = c_compiler.has_header('ntdef.h')
libssh2_have_ntstatus_h = c_compiler.has_header('ntstatus.h')

c_cond = '''
#include <@0@>

#if !(@1@)
#error "unsatisfied condition: @1@"
#endif

int main(void) {
  return 0;
}
'''

libssh2_have_o_nonblock = false
if libssh2_have_fcntl_h == true
    if c_compiler.links(c_cond.format('fcntl.h', 'defined O_NONBLOCK'))
        libssh2_have_o_nonblock = true
    endif
endif

libssh2_have_fionbio = false
if libssh2_have_sys_ioctl_h == true
    if c_compiler.links(c_cond.format('ioctl.h', 'defined FIONBIO'))
        libssh2_have_fionbio = true
    endif
endif

if with_openssl == true
    libssh2_have_EVP_aes_128_ctr = c_compiler.has_function('EVP_aes_128_ctr')
endif

libssh2_have_gettimeofday = c_compiler.has_function('gettimeofday')
libssh2_have_inet_addr = c_compiler.has_function('inet_addr')
libssh2_have_poll = c_compiler.has_function('poll')
libssh2_have_select = c_compiler.has_function('select')
libssh2_have_socket = c_compiler.has_function('socket')
libssh2_have_strtoll = c_compiler.has_function('strtoll')
libssh2_have_strtoi64 = c_compiler.has_function('strtoi64')
libssh2_have_snprintf = c_compiler.has_function('sprintf')

libssh2_config_private_data = configuration_data()
libssh2_config_private_data.set('HAVE_UNISTD_H', libssh2_have_unistd_h)
libssh2_config_private_data.set('HAVE_INTTYPES_H', libssh2_have_inttypes_h)
libssh2_config_private_data.set('HAVE_STDLIB_H', libssh2_have_stdlib_h)
libssh2_config_private_data.set('HAVE_SYS_SELECT_H', libssh2_have_sys_select_h)
libssh2_config_private_data.set('HAVE_SYS_UIO_H', libssh2_have_sys_uio_h)
libssh2_config_private_data.set('HAVE_SYS_SOCKET_H', libssh2_have_sys_socket_h)
libssh2_config_private_data.set('HAVE_SYS_IOCTL_H', libssh2_have_sys_ioctl_h)
libssh2_config_private_data.set('HAVE_SYS_TIME_H', libssh2_have_sys_time_h)
libssh2_config_private_data.set('HAVE_SYS_UN_H', libssh2_have_sys_un_h)
libssh2_config_private_data.set('HAVE_WINDOWS_H', libssh2_have_windows_h)
libssh2_config_private_data.set('HAVE_WS2TCPIP_H', libssh2_have_ws2tcpip_h)
libssh2_config_private_data.set('HAVE_WINSOCK2_H', libssh2_have_winsock2_h)
libssh2_config_private_data.set('HAVE_NTDEF_H', libssh2_have_ntdef_h)
libssh2_config_private_data.set('HAVE_NTSTATUS_H', libssh2_have_ntstatus_h)
libssh2_config_private_data.set('HAVE_O_NONBLOCK', libssh2_have_o_nonblock)
libssh2_config_private_data.set('HAVE_FIONBIO', libssh2_have_fionbio)
libssh2_config_private_data.set('HAVE_EVP_AES_128_CTR', libssh2_have_EVP_aes_128_ctr)
libssh2_config_private_data.set('HAVE_GETTIMEOFDAY', libssh2_have_gettimeofday)
libssh2_config_private_data.set('HAVE_INET_ADDR', libssh2_have_gettimeofday)
libssh2_config_private_data.set('HAVE_POLL', libssh2_have_poll)
libssh2_config_private_data.set('HAVE_SELECT', libssh2_have_select)
libssh2_config_private_data.set('HAVE_SOCKET', libssh2_have_socket)
libssh2_config_private_data.set('HAVE_STRTOLL', libssh2_have_strtoll)
libssh2_config_private_data.set('HAVE_STRTOI64', libssh2_have_strtoi64)
libssh2_config_private_data.set('HAVE_SNPRINTF', libssh2_have_snprintf)
libssh2_config_private_data.set('HAVE_LONGLONG', 1)
libssh2_config_private_data.set('LIBSSH2DEBUG', 1)

libssh2_config_private_h = configure_file(input: 'src/libssh2_config_cmake.h.in',
    output: 'libssh2_config.h',
    configuration: libssh2_config_private_data,
    format: 'cmake')
libssh2_sources += [libssh2_config_private_h]

internal_inc = include_directories('include')
libssh2_lib = library('ssh2',
    libssh2_sources,
    include_directories: internal_inc,
    dependencies: libssh2_deps,
    c_args: extra_flags
)
libssh2_dep = declare_dependency(
    include_directories: internal_inc,
    link_with: libssh2_lib
)

# fwd_example = executable('ssh-fwd',
#     'example/tcpip-forward.c',
#     dependencies: libssh2_dep
# )

# direct_tcpip_example = executable('ssh-tcpip-fwd',
#     'example/direct_tcpip.c',
#     dependencies: libssh2_dep
# )