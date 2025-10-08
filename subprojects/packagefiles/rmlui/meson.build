cmake = import('cmake')

cmake_opts = cmake.subproject_options()
cmake_opts.add_cmake_defines({
  'BUILD_SHARED_LIBS': 'OFF',
  'RMLUI_BUILD_LUA_BINDINGS': 'OFF',
  'RMLUI_BUILD_SAMPLES': 'OFF',
  'RMLUI_BUILD_TESTS': 'OFF',
  'RMLUI_ENABLE_PRECOMPILED_HEADERS': 'OFF',
})

freetype_dep = dependency('freetype2', required: false)
if freetype_dep.found()
  freetype_prefix = ''
  if freetype_dep.type_name() == 'pkgconfig'
    freetype_prefix = freetype_dep.get_pkgconfig_variable('prefix', default: '')
  elif freetype_dep.type_name() == 'cmake'
    freetype_prefix = freetype_dep.get_variable(cmake: 'PACKAGE_PREFIX_DIR', default: '')
  endif

  if freetype_prefix != ''
    cmake_opts.add_cmake_defines({'FREETYPE_ROOT': freetype_prefix})
  endif
endif

cmake_proj = cmake.configure('rmlui', options: cmake_opts)
