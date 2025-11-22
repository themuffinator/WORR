windows = import('windows')

common_src += files('system.cpp', 'hunk.cpp')
client_src += files('client.cpp', 'wgl.cpp', 'ac.cpp')

common_deps += cc.find_library('ws2_32')
client_deps += cc.find_library('opengl32')
xinput_lib = cc.find_library('xinput', required: false)
if not xinput_lib.found()
  xinput_lib = cc.find_library('xinput9_1_0')
endif
client_deps += xinput_lib

rc_args = ['-DHAVE_CONFIG_H']
rc_inc = meson.project_build_root()

client_src += windows.compile_resources('res/q2pro.rc',
  args:                rc_args,
  depend_files:        ['res/q2pro.ico', 'res/q2pro.exe.manifest'],
  include_directories: rc_inc,
)

server_src += windows.compile_resources('res/q2proded.rc',
  args:                rc_args,
  depend_files:        ['res/q2proded.ico', 'res/q2proded.exe.manifest'],
  include_directories: rc_inc,
)

game_src += windows.compile_resources('res/game.rc',
  args:                rc_args,
  include_directories: rc_inc,
)

client_deps += subproject('khr-headers').get_variable('khr_headers_dep')

if get_option('software-sound').allowed()
  client_src += files('wave.cpp', 'dsound.cpp')
  client_deps += cc.find_library('winmm')
endif

if get_option('windows-egl')
  client_src += files('egl.cpp')
  config.set10('USE_WIN32EGL', true)
endif

if get_option('windows-service')
  config.set('USE_WINSVC', 'USE_SERVER')
endif

if get_option('windows-crash-dumps').require(host_machine.cpu_family().startswith('x86')).allowed()
  common_src += files('debug.cpp')
  config.set10('USE_DBGHELP', true)
endif
