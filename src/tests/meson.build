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
    '../agent/tests/upload.cc'
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

test('Command', gtest_all, args: ['--gtest_filter=Command.*'])
test('CommandHandlerTest', gtest_all, args: ['--gtest_filter=CommandHandlerTest.*'])
test('CommandHandlerTestAsync', gtest_all, args: ['--gtest_filter=CommandHandlerTestAsync.*'])
test('nlohmann_json', gtest_all, args: ['--gtest_filter=nlohmann_json.*'])
test('agent_manager_test', gtest_all, args: ['--gtest_filter=agent_manager_test.*'])
test('base_command', gtest_all, args: ['--gtest_filter=base_command.*'])
test('TestProperties', gtest_all, args: ['--gtest_filter=TestProperties.*'])
test('utils', gtest_all, args: ['--gtest_filter=utils.*-utils.LoggingFileReset'])
test('uploader_test', gtest_all, args: ['--gtest_filter=uploader_test.*'])
