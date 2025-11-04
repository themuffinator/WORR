cmake = import('cmake')

cmake_opts = cmake.subproject_options()
cmake_opts.add_cmake_defines({
  'BUILD_SHARED_LIBS': 'OFF',
  'CMAKE_DISABLE_FIND_PACKAGE_BZip2': 'ON',
  'CMAKE_DISABLE_FIND_PACKAGE_BrotliDec': 'ON',
  'CMAKE_DISABLE_FIND_PACKAGE_HarfBuzz': 'ON',
  'CMAKE_DISABLE_FIND_PACKAGE_PNG': 'ON',
})

zlib_dep = dependency('zlib',
  required: false,
  fallback: ['zlib-ng', 'zlib_ng_dep'],
)
cmake_opts.add_cmake_defines({
  'CMAKE_DISABLE_FIND_PACKAGE_ZLIB': zlib_dep.found() ? 'OFF' : 'ON',
})

cmake_proj = cmake.configure('freetype2', options: cmake_opts)

cmake_freetype_dep = cmake_proj.dependency('freetype')
freetype_dep = declare_dependency(
  include_directories: cmake_freetype_dep.get_include_dirs(),
  compile_args: cmake_freetype_dep.get_compile_args(),
  link_args: cmake_freetype_dep.get_link_args(),
  link_with: cmake_freetype_dep.libraries,
  dependencies: cmake_freetype_dep.ext_deps,
  objects: cmake_freetype_dep.objects,
  sources: cmake_freetype_dep.get_sources(),
  extra_files: cmake_freetype_dep.get_extra_files(),
)
