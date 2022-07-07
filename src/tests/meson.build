gtest_deps = [
    dependency('gtest', fallback: ['gtest', 'gtest_dep']),
    dependency('gmock', fallback: ['gtest', 'gmock_dep']),
    dependency('nlohmann_json', fallback: ['nlohmann_json', 'nlohmann_json_dep']),
    dependency('xml2json', fallback: ['xml2json', 'xml2json_dep'])
]

core_srcs = [
    '../agent-proto/command/command.cc',
    '../agent-proto/command/utils.cc',
    '../agent-proto/command-handler.cc',
    '../agent/manager.cc',

    '../utils/packbits.c',
    '../utils/logging.cc',
    '../utils/utils.cc',
    '../utils/base64.cc',
    '../utils/properties.cc',

    '../net/websockets.cc',
    '../net/http.cc',
]

tests_src = [
    'gtest-all.cc',
    'test_Properties.cc',
    'test_Transport.cc',
    'test_Utils.cc',
    'test_http.cc',
    'test_Timeline.cc',
    '../agent-proto/tests/test-command.cc',
    '../agent-proto/tests/test-command-handler.cc',
    '../agent/tests/manager.cc',
    '../agent/tests/upload.cc',
    '../agent/tests/event-state.cc',
    '../agent/tests/event-manager.cc',
    '../agent/tests/queued-async-storage.cc',
]

gtest_all = executable(
    'gtest-all',
        [ tests_src, core_srcs ],
    include_directories: vxgcloudagent_includes,
    cpp_args: '-DDEFAULT_PROPS_FILE="/tmp/test.props"',
    dependencies : [
        gtest_deps, deps, vxgcloudagent_dep
    ],
)

test('Command', gtest_all, args: ['--gtest_filter=Command.*'], protocol: 'gtest')
test('CommandHandlerTest', gtest_all, args: ['--gtest_filter=CommandHandlerTest.*'], protocol: 'gtest')
test('CommandHandlerTestAsync', gtest_all, args: ['--gtest_filter=CommandHandlerTestAsync.*'], protocol: 'gtest')
test('nlohmann_json', gtest_all, args: ['--gtest_filter=nlohmann_json.*'], protocol: 'gtest')
test('agent_manager_test', gtest_all, args: ['--gtest_filter=agent_manager_test.*'], protocol: 'gtest')
test('base_command', gtest_all, args: ['--gtest_filter=base_command.*'], protocol: 'gtest')
test('TestProperties', gtest_all, args: ['--gtest_filter=TestProperties.*'], protocol: 'gtest')
test('utils', gtest_all, args: ['--gtest_filter=utils.*-utils.LoggingFileReset'], protocol: 'gtest')
test('uploader_test', gtest_all, args: ['--gtest_filter=uploader_test.*'], protocol: 'gtest')
test('WSTest', gtest_all, args: ['--gtest_filter=WSTest.timed_callbacks_test*'], protocol: 'gtest')
test('event_manager', gtest_all, args: ['--gtest_filter=event_manage_test.*'], protocol: 'gtest')
test('event_state', gtest_all, args: ['--gtest_filter=event_state_test.*'], protocol: 'gtest')
test('queued_async_storage', gtest_all, args: ['--gtest_filter=queued_async_storage_test.*'], protocol: 'gtest')

valgrind = find_program('valgrind', required : false)
if valgrind.found()
    valgrind_env = environment()
    # valgrind_suppressions_file = dir_src_test / 'valgrind.suppressions'
    add_test_setup('valgrind',
            exe_wrapper : [ valgrind,
                    '--leak-check=full',
                    '--show-leak-kinds=all',
                    '--num-callers=32',
                    '--gen-suppressions=all',
                    '--error-exitcode=3',
                    # '--suppressions=' + valgrind_suppressions_file
                    ],
            env :  valgrind_env,
            timeout_multiplier : 100)
else
    message('valgrind not found, disabling valgrind test suite')
endif
