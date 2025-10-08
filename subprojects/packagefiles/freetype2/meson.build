cmake = import('cmake')
use_cmake_option_objects = meson.version().version_compare('>=1.2.0')

if use_cmake_option_objects
  cmake_opts = cmake.subproject_options()
  cmake_opts.add_cmake_defines({
    'BUILD_SHARED_LIBS': 'OFF',
    'CMAKE_DISABLE_FIND_PACKAGE_BZip2': 'ON',
    'CMAKE_DISABLE_FIND_PACKAGE_BrotliDec': 'ON',
    'CMAKE_DISABLE_FIND_PACKAGE_HarfBuzz': 'ON',
    'CMAKE_DISABLE_FIND_PACKAGE_PNG': 'ON',
  })
else
  cmake_opts = [
    '-DBUILD_SHARED_LIBS=OFF',
    '-DCMAKE_DISABLE_FIND_PACKAGE_BZip2=ON',
    '-DCMAKE_DISABLE_FIND_PACKAGE_BrotliDec=ON',
    '-DCMAKE_DISABLE_FIND_PACKAGE_HarfBuzz=ON',
    '-DCMAKE_DISABLE_FIND_PACKAGE_PNG=ON',
  ]
endif

zlib_dep = dependency('zlib', required: false)
if use_cmake_option_objects
  cmake_opts.add_cmake_defines({
    'CMAKE_DISABLE_FIND_PACKAGE_ZLIB': zlib_dep.found() ? 'OFF' : 'ON',
  })
else
  cmake_opts += ['-DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=' + (zlib_dep.found() ? 'OFF' : 'ON')]
endif

if use_cmake_option_objects
  cmake_proj = cmake.configure('freetype2', options: cmake_opts)
else
  cmake_proj = cmake.configure('freetype2', cmake_options: cmake_opts)
endif
