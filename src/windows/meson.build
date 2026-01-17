windows = import('windows')

common_src += files('system.c', 'hunk.c')
client_src += files('client.c', 'wgl.c')

common_deps += cc.find_library('ws2_32')
client_deps += cc.find_library('opengl32')

rc_args = ['-DHAVE_CONFIG_H']
rc_inc = meson.project_build_root()

rc_prog = find_program('rc', required: false)
windres_prog = find_program('windres', required: false)
llvm_rc_prog = find_program('llvm-rc', required: false)
use_windows_rc = rc_prog.found() or windres_prog.found()

llvm_rc_args = []
foreach arg : rc_args
  if arg.startswith('-D')
    llvm_rc_args += '/D' + arg.substring(2)
  else
    llvm_rc_args += arg
  endif
endforeach
llvm_rc_args += ['/I' + rc_inc]

if not use_windows_rc and not llvm_rc_prog.found()
  warning('No Windows resource compiler found; resource files will be skipped')
endif

if use_windows_rc
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
elif llvm_rc_prog.found()
  client_src += custom_target('rc_worr',
    input:        'res/worr.rc',
    output:       'worr.res',
    depend_files: ['res/worr.ico', 'res/worr.exe.manifest'],
    command:      [llvm_rc_prog, '/FO@OUTPUT@'] + llvm_rc_args + ['@INPUT@'],
  )

  server_src += custom_target('rc_worrded',
    input:        'res/worrded.rc',
    output:       'worrded.res',
    depend_files: ['res/worrded.ico', 'res/worrded.exe.manifest'],
    command:      [llvm_rc_prog, '/FO@OUTPUT@'] + llvm_rc_args + ['@INPUT@'],
  )

  sgame_src += custom_target('rc_sgame',
    input:   'res/sgame.rc',
    output:  'sgame.res',
    command: [llvm_rc_prog, '/FO@OUTPUT@'] + llvm_rc_args + ['@INPUT@'],
  )

  cgame_src += custom_target('rc_cgame',
    input:   'res/cgame.rc',
    output:  'cgame.res',
    command: [llvm_rc_prog, '/FO@OUTPUT@'] + llvm_rc_args + ['@INPUT@'],
  )
endif

client_deps += khr_headers_dep

if get_option('bootstrapper')
  if use_windows_rc
    updater_src += windows.compile_resources('res/worrupdater.rc',
      args:                rc_args,
      depend_files:        ['res/worr.ico', 'res/worrupdater.exe.manifest'],
      include_directories: rc_inc,
    )
  elif llvm_rc_prog.found()
    updater_src += custom_target('rc_worrupdater',
      input:        'res/worrupdater.rc',
      output:       'worrupdater.res',
      depend_files: ['res/worr.ico', 'res/worrupdater.exe.manifest'],
      command:      [llvm_rc_prog, '/FO@OUTPUT@'] + llvm_rc_args + ['@INPUT@'],
    )
  endif
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
