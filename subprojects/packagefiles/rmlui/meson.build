cmake = import('cmake')
use_cmake_option_objects = meson.version().version_compare('>=1.2.0')

if use_cmake_option_objects
  cmake_opts = cmake.subproject_options()
  cmake_opts.add_cmake_defines({
    'BUILD_SHARED_LIBS': 'OFF',
    'RMLUI_BUILD_LUA_BINDINGS': 'OFF',
    'RMLUI_BUILD_SAMPLES': 'OFF',
    'RMLUI_BUILD_TESTS': 'OFF',
    'RMLUI_ENABLE_PRECOMPILED_HEADERS': 'OFF',
  })
else
  cmake_opts = [
    '-DBUILD_SHARED_LIBS=OFF',
    '-DRMLUI_BUILD_LUA_BINDINGS=OFF',
    '-DRMLUI_BUILD_SAMPLES=OFF',
    '-DRMLUI_BUILD_TESTS=OFF',
    '-DRMLUI_ENABLE_PRECOMPILED_HEADERS=OFF',
  ]
endif

freetype_dep = dependency('freetype2', required: false)
if freetype_dep.found()
  freetype_prefix = ''
  if freetype_dep.type_name() == 'pkgconfig'
    freetype_prefix = freetype_dep.get_pkgconfig_variable('prefix', default: '')
  elif freetype_dep.type_name() == 'cmake'
    freetype_prefix = freetype_dep.get_variable(cmake: 'PACKAGE_PREFIX_DIR', default: '')
  endif

  if freetype_prefix != ''
    if use_cmake_option_objects
      cmake_opts.add_cmake_defines({'FREETYPE_ROOT': freetype_prefix})
    else
      cmake_opts += ['-DFREETYPE_ROOT=' + freetype_prefix]
    endif
  endif
endif

if use_cmake_option_objects
  cmake_proj = cmake.configure('rmlui', options: cmake_opts)
else
  cmake_proj = cmake.configure('rmlui', cmake_options: cmake_opts)
endif
