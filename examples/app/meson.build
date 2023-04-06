project('test_app', 'cpp',
  version : '0.1',
  meson_version : '>= 0.56.0',
  default_options : [ 'warning_level=1',
                      'buildtype=debug',
                      'auto_features=disabled',
                      'default_library=static',
                      'cpp_std=c++11', 'cpp_link_args=-static-libstdc++' ])

cloudagent = dependency('vxgcloudagent')
args = dependency('args')

dep = [cloudagent, args]

# //! [meson-example-target]
cloud_agent_minimal = executable('cloud-agent-minimal', 'src/cloud-agent-minimal.cc',
  install : true, dependencies: dep)
# //! [meson-example-target]

cloud_agent = executable('cloud-agent', 'src/cloud-agent.cc',
  install : true, dependencies: dep)
