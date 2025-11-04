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
if zlib_dep.found()
  cmake_opts.add_cmake_dep('ZLIB', zlib_dep)
endif
cmake_opts.add_cmake_defines({
  'CMAKE_DISABLE_FIND_PACKAGE_ZLIB': zlib_dep.found() ? 'OFF' : 'ON',
})

cmake_proj = cmake.configure('freetype2', options: cmake_opts)
