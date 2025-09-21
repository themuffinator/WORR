project('q2proto', 'c',
  license: 'GPL-2.0-or-later',
  meson_version: '>= 0.59.0',
  default_options: [
    'c_std=c11',
    'buildtype=debugoptimized',
  ],
)

q2proto_src = [
  '../src/q2proto_client.c',
  '../src/q2proto_coords.c',
  '../src/q2proto_crc.c',
  '../src/q2proto_error.c',
  '../src/q2proto_internal_common.c',
  '../src/q2proto_internal_debug.c',
  '../src/q2proto_internal_download.c',
  '../src/q2proto_internal_fmt.c',
  '../src/q2proto_internal_maybe_zpacket.c',
  '../src/q2proto_internal_packing.c',
  '../src/q2proto_multicast.c',
  '../src/q2proto_proto_kex.c',
  '../src/q2proto_proto_q2pro_extdemo.c',
  '../src/q2proto_proto_q2pro.c',
  '../src/q2proto_proto_q2repro.c',
  '../src/q2proto_proto_r1q2.c',
  '../src/q2proto_proto_vanilla.c',
  '../src/q2proto_protocol.c',
  '../src/q2proto_server.c',
  '../src/q2proto_solid.c',
  '../src/q2proto_sound.c',
  '../src/q2proto_string.c',
]
dummy_src = [
  '../src/dummy_q2protodbg_shownet.c',
  '../src/dummy_q2protoerr_client_read.c',
  '../src/dummy_q2protoerr_client_write.c',
  '../src/dummy_q2protoerr_server_read.c',
  '../src/dummy_q2protoerr_server_write.c',
  '../src/dummy_q2protoio_get_error.c',
  '../src/dummy_q2protoio_read.c',
  '../src/dummy_q2protoio_write.c',
]
dummy_deflate_src = [
  '../src/dummy_q2protoio_deflate.c',
  '../src/dummy_q2protoio_inflate.c',
]

q2proto_inc = include_directories('../inc')

cc = meson.get_compiler('c')

common_args = []
if cc.get_argument_syntax() == 'gcc'
  test_args = [
    '-fsigned-char',
    '-fms-extensions',
    '-fno-math-errno',
    '-fno-trapping-math',
    '-Wpointer-arith',
    '-Wformat-security',
    '-Werror=vla',
    '-Wstrict-prototypes',
    '-Wmissing-prototypes',
  ]

  if cc.get_id() == 'clang'
    test_args += ['-Wno-microsoft-anon-tag']
  endif

  common_args += cc.get_supported_arguments(test_args)

endif

tests_inc = [q2proto_inc, 'inc']

add_project_arguments(common_args, language: 'c')
test_flavors = ['vanilla', 'q2pro_ext', 'q2pro_ext_v2', 'q2repro']
foreach flavor : test_flavors
  flavor_src = [f'build_@flavor@/build_@flavor@.c']
  flavor_inc = f'build_@flavor@'

  executable(f'build_@flavor@', q2proto_src, dummy_src, flavor_src,
    include_directories:   tests_inc + [flavor_inc],
    gnu_symbol_visibility: 'hidden',
    win_subsystem:         'console,6.0',
    c_args:                [],
  )

  executable(f'build_@flavor@_deflate', q2proto_src, dummy_src, dummy_deflate_src, flavor_src,
    include_directories:   tests_inc + [flavor_inc],
    gnu_symbol_visibility: 'hidden',
    win_subsystem:         'console,6.0',
    c_args:                ['-DQ2PROTO_COMPRESSION_DEFLATE=1'],
  )
endforeach

build_single_source_src = [
  'build_single_source/main.c',
  'build_single_source/repro.c',
  'build_single_source/vanilla.c'
  ]
executable(f'build_single_source', dummy_src, build_single_source_src,
  include_directories:   tests_inc + ['build_single_source', '../src'],
  gnu_symbol_visibility: 'hidden',
  win_subsystem:         'console,6.0',
  c_args:                [],
)
