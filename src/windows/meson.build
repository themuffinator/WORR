windows = import('windows')

common_src += files('system.c', 'hunk.c')
client_src += files('client.c', 'wgl.c')

common_deps += cc.find_library('ws2_32')
client_deps += cc.find_library('opengl32')

rc_args = ['-DHAVE_CONFIG_H']
rc_inc = meson.project_build_root()

client_src += windows.compile_resources('res/worr.rc',
  args:                rc_args,
  depend_files:        ['res/worr.ico', 'res/worr.exe.manifest'],
  include_directories: rc_inc,
)

server_src += windows.compile_resources('res/worrded.rc',
  args:                rc_args,
  depend_files:        ['res/worrded.ico', 'res/worrded.exe.manifest'],
  include_directories: rc_inc,
)

sgame_src += windows.compile_resources('res/sgame.rc',
  args:                rc_args,
  include_directories: rc_inc,
)

cgame_src += windows.compile_resources('res/cgame.rc',
  args:                rc_args,
  include_directories: rc_inc,
)

client_deps += khr_headers_dep

if get_option('bootstrapper')
  updater_src += windows.compile_resources('res/worrupdater.rc',
    args:                rc_args,
    depend_files:        ['res/worr.ico', 'res/worrupdater.exe.manifest'],
    include_directories: rc_inc,
  )
  updater_deps += [
    cc.find_library('winhttp'),
    cc.find_library('comctl32'),
    cc.find_library('shlwapi'),
    cc.find_library('bcrypt'),
    cc.find_library('version'),
  ]
endif

if get_option('software-sound').allowed()
  client_src += files('wave.c', 'dsound.c')
  client_deps += cc.find_library('winmm')
endif

if get_option('windows-egl')
  client_src += files('egl.c')
  config.set10('USE_WIN32EGL', true)
endif

if get_option('windows-service')
  config.set('USE_WINSVC', 'USE_SERVER')
endif

if get_option('windows-crash-dumps').require(host_machine.cpu_family().startswith('x86')).allowed()
  common_src += files('debug.c')
  config.set10('USE_DBGHELP', true)
endif
