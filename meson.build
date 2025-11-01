project('worr', ['cpp', 'c'],
  license: 'GPL-2.0-or-later',
  version: run_command(find_program('python3'), 'version.py', check: true).stdout().strip(),
  meson_version: '>= 0.60.0',
  default_options: [
    meson.version().version_compare('>=1.3.0') ? 'c_std=gnu11,c11' : 'c_std=gnu11',
    'cpp_std=c++20',
    'buildtype=debugoptimized',
  ],
)

add_project_arguments('-DPNG_RESTRICT=__restrict', language : 'cpp')

cpp_compiler = meson.get_compiler('cpp')

if cpp_compiler.get_id() != 'msvc'
  cpp_latest_flags = []
  if cpp_compiler.has_argument('-std=c++2b')
    cpp_latest_flags += ['-std=c++2b']
  elif cpp_compiler.has_argument('-std=c++2a')
    cpp_latest_flags += ['-std=c++2a']
  endif

  if cpp_latest_flags.length() > 0
    add_project_arguments(cpp_latest_flags, language: 'cpp')
  endif
endif


c_compiler = meson.get_compiler('c')

common_src = [
  'src/common/bsp.cpp',
  'src/common/cmd.cpp',
  'src/common/cmodel.cpp',
  'src/common/common.cpp',
  'src/common/crc.cpp',
  'src/common/cvar.cpp',
  'src/common/error.cpp',
  'src/common/field.cpp',
  'src/common/fifo.cpp',
  'src/common/files.cpp',
  'src/common/game3_convert.cpp',
  'src/common/game3_pmove/new.cpp',
  'src/common/game3_pmove/old.cpp',
  'src/common/natsort.cpp',
  'src/common/hash_map.cpp',
  'src/common/loc.cpp',
  'src/common/math.cpp',
  'src/common/mdfour.cpp',
  'src/common/msg.cpp',
  'src/common/net/chan.cpp',
  'src/common/net/net.cpp',
  'src/common/pmove.cpp',
  'src/common/prompt.cpp',
  'src/common/q2proto_shared.cpp',
  'src/common/sizebuf.cpp',
  'src/common/steam.cpp',
  'src/common/utils.cpp',
  'src/common/zone.cpp',
  'src/common/features.hpp',
  'src/common/mapdb.cpp',
  'src/shared/shared.cpp',

  'q2proto/src/q2proto_client.c',
  'q2proto/src/q2proto_coords.c',
  'q2proto/src/q2proto_crc.c',
  'q2proto/src/q2proto_error.c',
  'q2proto/src/q2proto_internal_common.c',
  'q2proto/src/q2proto_internal_debug.c',
  'q2proto/src/q2proto_internal_download.c',
  'q2proto/src/q2proto_internal_fmt.c',
  'q2proto/src/q2proto_internal_maybe_zpacket.c',
  'q2proto/src/q2proto_internal_packing.c',
  'q2proto/src/q2proto_multicast.c',
  'q2proto/src/q2proto_proto_kex.c',
  'q2proto/src/q2proto_proto_q2pro_extdemo.c',
  'q2proto/src/q2proto_proto_q2pro.c',
  'q2proto/src/q2proto_proto_q2repro.c',
  'q2proto/src/q2proto_proto_r1q2.c',
  'q2proto/src/q2proto_proto_vanilla.c',
  'q2proto/src/q2proto_protocol.c',
  'q2proto/src/q2proto_server.c',
  'q2proto/src/q2proto_solid.c',
  'q2proto/src/q2proto_sound.c',
  'q2proto/src/q2proto_string.c',

  'src/common/q2proto-glue.cpp',
  'src/common/q2proto-packing.cpp',

  'inc/shared/atomic.hpp',
  'inc/shared/base85.hpp',
  'inc/shared/files.hpp',
  'inc/shared/game.hpp',
  'inc/shared/game3_shared.hpp',
  'inc/shared/list.hpp',
  'inc/shared/m_flash.hpp',
  'inc/shared/platform.hpp',
  'inc/shared/shared.hpp',

  'inc/common/natsort.hpp',
  'inc/common/async.hpp',
  'inc/common/bsp.hpp',
  'inc/common/cmd.hpp',
  'inc/common/cmodel.hpp',
  'inc/common/common.hpp',
  'inc/common/cvar.hpp',
  'inc/common/error.hpp',
  'inc/common/field.hpp',
  'inc/common/fifo.hpp',
  'inc/common/files.hpp',
  'inc/common/gamedll.hpp',
  'inc/common/hash_map.hpp',
  'inc/common/intreadwrite.hpp',
  'inc/common/loc.hpp',
  'inc/common/math.hpp',
  'inc/common/mdfour.hpp',
  'inc/common/msg.hpp',
  'inc/common/pmove.hpp',
  'inc/common/prompt.hpp',
  'inc/common/protocol.hpp',
  'inc/common/sizebuf.hpp',
  'inc/common/steam.hpp',
  'inc/common/tests.hpp',
  'inc/common/utils.hpp',
  'inc/common/zone.hpp',

  'inc/common/net/chan.hpp',
  'inc/common/net/net.hpp',

  'inc/format/bsp.hpp',
  'inc/format/md2.hpp',
  'inc/format/md3.hpp',
  'inc/format/pak.hpp',
  'inc/format/pcx.hpp',
  'inc/format/sp2.hpp',
  'inc/format/wal.hpp',

  'inc/system/hunk.hpp',
  'inc/system/pthread.hpp',
  'inc/system/system.hpp',
]

client_src = [
  'src/client/ascii.cpp',
  'src/client/cgame.cpp',
  'src/client/cgame_classic.cpp',
  'src/client/console.cpp',
  'src/client/demo.cpp',
  'src/client/ffmpeg_utils.cpp',
  'src/client/download.cpp',
  'src/client/effects.cpp',
  'src/client/entities.cpp',
  'src/client/input.cpp',
  'src/client/keys.cpp',
  'src/client/locs.cpp',
  'src/client/main.cpp',
  'src/client/newfx.cpp',
  'src/client/parse.cpp',
  'src/client/precache.cpp',
  'src/client/predict.cpp',
  'src/client/refresh.cpp',
  'src/client/screen.cpp',
  'src/client/sound/main.cpp',
  'src/client/sound/mem.cpp',
  'src/client/sound/qal.hpp',
  'src/client/sound/sound.hpp',
  'src/client/tent.cpp',
  'src/client/view.cpp',
  'src/client/wheel.cpp',
  'src/client/client.hpp',
  'src/client/ffmpeg_utils.hpp',
  'src/client/cgame_classic.hpp',
  'src/common/async.cpp',
  'src/common/gamedll.cpp',
  'src/server/commands.cpp',
  'src/server/entities.cpp',
  'src/server/game.cpp',
  'src/server/game3_proxy/game3_proxy.cpp',
  'src/server/init.cpp',
  'src/server/main.cpp',
  'src/server/send.cpp',
  'src/server/user.cpp',
  'src/server/nav.cpp',
  'src/server/world.cpp',
  'src/server/server.hpp',
  'src/shared/base85.cpp',
  'src/shared/m_flash.cpp',

  'inc/server/server.hpp',

  'inc/client/client.hpp',
  'inc/client/input.hpp',
  'inc/client/keys.hpp',
  'inc/client/proc_address.hpp',
  'inc/client/ui.hpp',
  'inc/client/video.hpp',

  'inc/client/sound/dma.hpp',
  'inc/client/sound/sound.hpp',
]

ui_src = [
  'src/client/ui/demos.cpp',
  'src/client/ui/menu.cpp',
  'src/client/ui/playerconfig.cpp',
  'src/client/ui/playermodels.cpp',
  'src/client/ui/script.cpp',
  'src/client/ui/servers.cpp',
  'src/client/ui/mapdb.cpp',
  'src/client/ui/ui.cpp',
  'src/client/ui/ui.hpp',
]

refresh_src = [
  'src/refresh/draw.cpp',
  'src/refresh/hq2x.cpp',
  'src/refresh/images.cpp',
  'src/refresh/legacy.cpp',
  'src/refresh/main.cpp',
  'src/refresh/mesh.cpp',
  'src/refresh/models.cpp',
  'src/refresh/qgl.cpp',
  'src/refresh/shader.cpp',
  'src/refresh/sky.cpp',
  'src/refresh/state.cpp',
  'src/refresh/surf.cpp',
  'src/refresh/tess.cpp',
  'src/refresh/texture.cpp',
  'src/refresh/world.cpp',
  'src/refresh/debug.cpp',
  'src/refresh/debug_text.cpp',
  'src/refresh/arbfp.hpp',
  'src/refresh/gl.hpp',
  'src/refresh/images.hpp',
  'src/refresh/qgl.hpp',

  'inc/refresh/refresh.hpp',
]

server_src = [
  'src/client/null.cpp',
  'src/common/gamedll.cpp',
  'src/server/commands.cpp',
  'src/server/entities.cpp',
  'src/server/game.cpp',
  'src/server/game3_proxy/game3_proxy.cpp',
  'src/server/init.cpp',
  'src/server/main.cpp',
  'src/server/send.cpp',
  'src/server/user.cpp',
  'src/server/nav.cpp',
  'src/server/world.cpp',
  'src/server/server.hpp',
  'src/shared/base85.cpp',

  'inc/server/server.hpp',
]

game_src = [
  'src/game/g_ai.cpp',
  'src/game/g_chase.cpp',
  'src/game/g_cmds.cpp',
  'src/game/g_combat.cpp',
  'src/game/g_func.cpp',
  'src/game/g_items.cpp',
  'src/game/g_main.cpp',
  'src/game/g_misc.cpp',
  'src/game/g_monster.cpp',
  'src/game/g_phys.cpp',
  'src/game/g_ptrs.cpp',
  'src/game/g_save.cpp',
  'src/game/g_spawn.cpp',
  'src/game/g_svcmds.cpp',
  'src/game/g_target.cpp',
  'src/game/g_trigger.cpp',
  'src/game/g_turret.cpp',
  'src/game/g_utils.cpp',
  'src/game/g_weapon.cpp',
  'src/game/m_actor.cpp',
  'src/game/m_berserk.cpp',
  'src/game/m_boss2.cpp',
  'src/game/m_boss31.cpp',
  'src/game/m_boss32.cpp',
  'src/game/m_boss3.cpp',
  'src/game/m_brain.cpp',
  'src/game/m_chick.cpp',
  'src/game/m_flipper.cpp',
  'src/game/m_float.cpp',
  'src/game/m_flyer.cpp',
  'src/game/m_gladiator.cpp',
  'src/game/m_gunner.cpp',
  'src/game/m_hover.cpp',
  'src/game/m_infantry.cpp',
  'src/game/m_insane.cpp',
  'src/game/m_medic.cpp',
  'src/game/m_move.cpp',
  'src/game/m_mutant.cpp',
  'src/game/m_parasite.cpp',
  'src/game/m_soldier.cpp',
  'src/game/m_supertank.cpp',
  'src/game/m_tank.cpp',
  'src/game/p_client.cpp',
  'src/game/p_hud.cpp',
  'src/game/p_trail.cpp',
  'src/game/p_view.cpp',
  'src/game/p_weapon.cpp',
  'src/shared/base85.cpp',
  'src/shared/m_flash.cpp',
  'src/shared/shared.cpp',
]

cc = meson.get_compiler('c')
cppc = meson.get_compiler('cpp')

win32 = host_machine.system() == 'windows'
x86 = host_machine.cpu_family() == 'x86'

cpuremap = {
  'x86': win32 ? 'x86' : 'i386',
  'aarch64': 'arm64',
}

cpu = host_machine.cpu_family()
if cpu in cpuremap
  cpu = cpuremap[cpu]
endif

if win32
  sys = cpu.endswith('64') ? 'Win64' : 'Win32'
else
  sys = host_machine.system()
  if sys.endswith('bsd')
    sys = sys.substring(0, -3) + sys.substring(-3).to_upper()
  endif
  sys = sys[0].to_upper() + sys.substring(1)
endif

bindir = '.'
datadir = '.'
libdir = '.'
homedir = ''
default_prefix = ''

system_wide = not win32 and get_option('system-wide')
if system_wide
  bindir = get_option('prefix') / get_option('bindir')
  datadir = get_option('prefix') / get_option('datadir') / meson.project_name()
  libdir = get_option('prefix') / get_option('libdir') / meson.project_name()
  homedir = get_option('homedir')
else
  default_prefix = get_option('prefix')
endif

common_args = ['-DHAVE_CONFIG_H', '-DQ2PROTO_CONFIG_H="common/q2proto_config.hpp"']
if win32
  common_args += '-D_USE_MATH_DEFINES'
else
  common_args += '-D_GNU_SOURCE'
endif

engine_args = []
if win32
  engine_args += '-D_WIN32_WINNT=0x0601'
endif

common_link_args = []
exe_link_args = []
dll_link_args = []
msvc_cpp_args = []

if cc.get_argument_syntax() == 'gcc'
  if x86
    foreach lang : ['c', 'cpp']
      add_global_arguments('-msse2', '-mfpmath=sse', language: lang)
    endforeach
  endif

  test_args = [
    '-Werror=vla',
    '-Wformat-security',
    '-Wno-microsoft-anon-tag',
    '-Wpointer-arith',
    '-Wstrict-prototypes',
    '-fms-extensions',
    '-fno-math-errno',
    '-fno-trapping-math',
    '-fsigned-char',
  ]

  common_args += cc.get_supported_arguments(test_args)
  common_args += cppc.get_supported_arguments(test_args)
  engine_args += cc.get_supported_arguments(['-Wmissing-prototypes'])

  if win32
    common_args += '-D__USE_MINGW_ANSI_STDIO=1'
    common_link_args += '-Wl,--nxcompat,--dynamicbase'

    if cpu == 'x86_64'
      exe_link_args += '-Wl,--high-entropy-va,--image-base=0x140000000'
      dll_link_args += '-Wl,--high-entropy-va,--image-base=0x180000000'
    endif

    dll_link_args += '-static-libgcc'
  endif
elif cc.get_id() == 'msvc'
  common_args += ['/wd4146', '/wd4244', '/wd4305', '/D_CRT_SECURE_NO_WARNINGS', '/wd4267', '/wd4018']

  if not meson.version().version_compare('>=1.3.0')
    if cppc.has_argument('/std:c++20')
      msvc_cpp_args += '/std:c++20'
    elif cppc.has_argument('/std:c++latest')
      msvc_cpp_args += '/std:c++latest'
    else
      error('MSVC does not support the required C++20 standard flag')
    endif
  endif
endif

foreach lang : ['c', 'cpp']
  add_project_arguments(common_args, language: lang)
  add_project_link_arguments(common_link_args, language: lang)
endforeach

if msvc_cpp_args.length() > 0
  add_project_arguments(msvc_cpp_args, language: 'cpp')
endif

config = configuration_data()

texture_formats = []

fallback_opt = ['default_library=static']

zlib = dependency('zlib',
  fallback:        'zlib-ng',
  required:        get_option('zlib'),
  default_options: fallback_opt + [ 'tests=disabled', 'zlib-compat=true', 'force-sse2=true' ],
)

if not zlib.found()
  warning('zlib not found, client will be unable to connect to protocol 35 servers')
endif

png = dependency('libpng',
  version:         '>= 1.6.11',
  required:        get_option('libpng'),
  default_options: fallback_opt,
)
if png.found()
  texture_formats += 'png'
endif

config.set10('USE_ZLIB', zlib.found())
config.set10('USE_PNG', png.found())

if host_machine.system() == 'windows' and c_compiler.get_id() == 'msvc' and png.found()
  python_mod = import('python')
  python3 = python_mod.find_installation()
endif

curl = dependency('libcurl',
  version:         '>= 7.68.0',
  required:        get_option('libcurl'),
  default_options: fallback_opt,
)

if curl.found()
  client_src += 'src/client/http.cpp'
  config.set10('USE_CURL', true)
endif

sdl2 = dependency('sdl2', required: get_option('sdl2'))
if sdl2.found()
  client_src += 'src/unix/video/sdl.cpp'
  config.set('USE_SDL', 'USE_CLIENT')
endif

common_deps = [zlib]
client_deps = [png, curl, sdl2]
server_deps = []
game_deps = [zlib]

jpeg = dependency('libjpeg',
  required:        get_option('libjpeg'),
  default_options: fallback_opt + [ 'jpeg-turbo=disabled', 'tests=disabled' ]
)
if jpeg.found()
  if jpeg.type_name() == 'internal'
    has_rgba = true
  else
    has_rgba = cc.has_header_symbol('jpeglib.h', 'JCS_EXT_RGBA',
      prefix:       '#include <stdio.h>',
      dependencies: jpeg,
      required:     get_option('libjpeg'))
  endif
  if has_rgba
    client_deps += jpeg
    texture_formats += 'jpg'
    config.set10('USE_JPG', true)
  endif
endif

if get_option('tga')
  texture_formats += 'tga'
endif

openal = dependency('openal',
  version:         '>= 1.23.1',
  required:        get_option('openal'),
  default_options: fallback_opt,
)
if openal.found()
  client_src += [ 'src/client/sound/al.cpp', 'src/client/sound/qal.cpp', 'inc/common/jsmn.hpp' ]
  client_deps += openal
  config.set10('USE_OPENAL', true)
endif

# require FFmpeg >= 5.1.3
ffmpeg_defaults = [
  'tests=disabled',
  'programs=disabled',
  'devices=disabled',
  'network=disabled',

  'avdevice=disabled',
  'avfilter=disabled',
  'postproc=disabled',

  # Start with a very small set of features, and bit by bit add what we need
  'protocols=disabled',
  'file_protocol=enabled',

  'hwaccels=disabled',
  'bsfs=disabled',

  'parsers=disabled',
  'vp3_parser=enabled',
  'vorbis_parser=enabled',

  'decoders=disabled',
  'idcin_decoder=enabled',
  'theora_decoder=enabled',
  'vorbis_decoder=enabled',
  'pcm_f16le_decoder=enabled',
  'pcm_f24le_decoder=enabled',
  'pcm_f32be_decoder=enabled',
  'pcm_f32le_decoder=enabled',
  'pcm_f64be_decoder=enabled',
  'pcm_f64le_decoder=enabled',
  'pcm_lxf_decoder=enabled',
  'pcm_s8_decoder=enabled',
  'pcm_s8_planar_decoder=enabled',
  'pcm_s16be_decoder=enabled',
  'pcm_s16be_planar_decoder=enabled',
  'pcm_s16le_decoder=enabled',
  'pcm_s16le_planar_decoder=enabled',
  'pcm_s24be_decoder=enabled',
  'pcm_s24daud_decoder=enabled',
  'pcm_s24le_decoder=enabled',
  'pcm_s24le_planar_decoder=enabled',
  'pcm_s32be_decoder=enabled',
  'pcm_s32le_decoder=enabled',
  'pcm_s32le_planar_decoder=enabled',
  'pcm_s64be_decoder=enabled',
  'pcm_s64le_decoder=enabled',
  'pcm_sga_decoder=enabled',
  'pcm_u8_decoder=enabled',
  'pcm_u16be_decoder=enabled',
  'pcm_u16le_decoder=enabled',
  'pcm_u24be_decoder=enabled',
  'pcm_u24le_decoder=enabled',
  'pcm_u32be_decoder=enabled',
  'pcm_u32le_decoder=enabled',
  'pcm_vidc_decoder=enabled',
  'vp3_decoder=enabled',

  'demuxers=disabled',
  'idcin_demuxer=enabled',
  'ogg_demuxer=enabled',
  'wav_demuxer=enabled',

  'encoders=disabled',
  'muxers=disabled',
]
avcodec_opt = get_option('avcodec')
avcodec = dependency('libavcodec', version: '>= 59.37.100', default_options: ffmpeg_defaults, required: avcodec_opt)
avformat = dependency('libavformat', version: '>= 59.27.100', default_options: ffmpeg_defaults, required: avcodec_opt)
avutil = dependency('libavutil', version: '>= 57.28.100', default_options: ffmpeg_defaults, required: avcodec_opt)
swresample = dependency('libswresample', version: '>= 4.7.100', default_options: ffmpeg_defaults, required: avcodec_opt)
swscale = dependency('libswscale', version: '>= 6.7.100', default_options: ffmpeg_defaults, required: avcodec_opt)
if cc.get_id() == 'msvc' and avcodec_opt.allowed()
  ffmpeg_required = avcodec_opt.enabled()
  if not avcodec.found()
    avcodec = cc.find_library('avcodec', required: ffmpeg_required)
  endif
  if not avformat.found()
    avformat = cc.find_library('avformat', required: ffmpeg_required)
  endif
  if not avutil.found()
    avutil = cc.find_library('avutil', required: ffmpeg_required)
  endif
  if not swresample.found()
    swresample = cc.find_library('swresample', required: ffmpeg_required)
  endif
  if not swscale.found()
    swscale = cc.find_library('swscale', required: ffmpeg_required)
  endif
endif
ffmpeg_deps_list = [
  ['avcodec', avcodec],
  ['avformat', avformat],
  ['avutil', avutil],
  ['swresample', swresample],
  ['swscale', swscale],
]

ffmpeg_all_found = true
foreach dep_pair : ffmpeg_deps_list
  ffmpeg_all_found = ffmpeg_all_found and dep_pair[1].found()
endforeach

ffmpeg_requires_conversion = false
ffmpeg_conversion_success = true
converted_ffmpeg_deps = {}

if ffmpeg_all_found and cc.get_id() == 'msvc' and avcodec_opt.allowed()
  coff_librarian = find_program(['lib', 'llvm-lib'], required: false)
  converted_archives = {}

  foreach dep_pair : ffmpeg_deps_list
    dep_name = dep_pair[0]
    dep_obj = dep_pair[1]

    if dep_obj.type_name() == 'internal'
      ffmpeg_requires_conversion = true

      if not coff_librarian.found()
        ffmpeg_conversion_success = false
        break
      endif

      preserved_link_args = []
      fallback_archives = []

      foreach link_arg : dep_obj.get_link_args()
        if link_arg.endswith('.a')
          fallback_archives += [link_arg]
        else
          preserved_link_args += [link_arg]
        endif
      endforeach

      if fallback_archives.length() == 0
        ffmpeg_conversion_success = false
        break
      endif

      converted_paths = []
      idx = 0
      foreach archive : fallback_archives
        idx += 1

        if archive in converted_archives
          convert_target = converted_archives[archive]
        else
          convert_name = 'ffmpeg_@0@_coff_@1@'.format(dep_name, idx.to_string())
          convert_target = custom_target(
            convert_name,
            input: archive,
            output: '@BASENAME@.lib',
            command: [coff_librarian, '/nologo', '/OUT:@OUTPUT@', '@INPUT@'],
            build_by_default: true,
          )
          converted_archives[archive] = convert_target
        endif

        converted_paths += [convert_target.full_path()]
      endforeach

      converted_ffmpeg_deps[dep_name] = declare_dependency(
        include_directories: dep_obj.get_include_directories(),
        compile_args: dep_obj.get_compile_args(),
        sources: dep_obj.get_sources(),
        dependencies: dep_obj.get_dependencies(),
        link_whole: dep_obj.get_link_whole(),
        link_args: preserved_link_args + converted_paths,
      )
    endif
  endforeach
endif

if ffmpeg_requires_conversion and not ffmpeg_conversion_success
  warning('FFmpeg fallback libraries require conversion to COFF, but the conversion failed; disabling FFmpeg support')
  ffmpeg_all_found = false
endif

if ffmpeg_all_found
  final_ffmpeg_deps = []
  foreach dep_pair : ffmpeg_deps_list
    dep_name = dep_pair[0]

    if dep_name in converted_ffmpeg_deps
      final_ffmpeg_deps += [converted_ffmpeg_deps[dep_name]]
    else
      final_ffmpeg_deps += [dep_pair[1]]
    endif
  endforeach

  client_src += ['src/client/cin.cpp', 'src/client/sound/ogg.cpp']
  client_deps += final_ffmpeg_deps

  if not ffmpeg_requires_conversion or ffmpeg_conversion_success
    config.set10('USE_AVCODEC', true)
  endif
endif

if win32
  subdir('src/windows')
else
  subdir('src/unix')
endif

if get_option('client-gtv')
  client_src += 'src/client/gtv.cpp'
  config.set10('USE_CLIENT_GTV', true)
endif

if get_option('client-ui')

  # embed worr.menu
  prog_embed = find_program('embed.py')

  q2pro_menu_c = custom_target(
      'worr.menu.cpp',
      output : 'worr.menu.cpp',
      input : 'src/client/ui/worr.menu.json',
      command : [prog_embed, '@INPUT@', '@OUTPUT@', 'res_q2pro_menu'],
  )
  client_src += q2pro_menu_c

  client_src += ui_src
  config.set10('USE_UI', true)
endif

if get_option('software-sound').require(win32 or sdl2.found()).allowed()
  client_src += 'src/client/sound/dma.cpp'
  if sdl2.found()
    client_src += 'src/unix/sound/sdl.cpp'
  endif
  config.set10('USE_SNDDMA', true)
elif not openal.found()
  warning('Neither software sound nor OpenAL enabled, this is not supported')
endif

if get_option('anticheat-server')
  server_src += 'src/server/ac.cpp'
  config.set('USE_AC_SERVER', 'USE_SERVER')
endif

if get_option('mvd-server')
  common_src += [
    'src/server/mvd.cpp',
    'inc/server/mvd/client.hpp',
    'inc/server/mvd/protocol.hpp'
  ]
  config.set10('USE_MVD_SERVER', true)
endif

if get_option('mvd-client')
  common_src += [
    'src/server/mvd/client.cpp',
    'src/server/mvd/game.cpp',
    'src/server/mvd/parse.cpp'
  ]
  config.set10('USE_MVD_CLIENT', true)
endif

if get_option('save-games')
  common_src += 'src/server/save.cpp'
  config.set10('USE_SAVEGAMES', true)
endif

if get_option('system-console')
  if not win32
    common_src += 'src/unix/tty.cpp'
  endif
  config.set10('USE_SYSCON', true)
endif

if get_option('tests')
  common_src += 'src/common/tests.cpp'
  config.set10('USE_TESTS', true)
endif

if get_option('debug')
  refresh_src += 'src/refresh/debug.cpp'
endif

if get_option('game-abi-hack').require(x86 and cc.get_id() == 'gcc' and cc.has_argument('-mstackrealign')).allowed()
  config.set10('USE_GAME_ABI_HACK', true)
  engine_args += '-mstackrealign'
endif

executable('worr', common_src, client_src, refresh_src,
  dependencies:          common_deps + client_deps,
  include_directories:   ['inc', 'q2proto/inc'],
  gnu_symbol_visibility: 'hidden',
  win_subsystem:         'windows,6.0',
  link_args:             exe_link_args,
  c_args:                ['-DUSE_CLIENT=1', '-DUSE_REF=1', engine_args],
  cpp_args:              ['-DUSE_CLIENT=1', '-DUSE_REF=1', engine_args],
  install:               true,
  install_dir:           bindir,
)

executable('worr.ded', common_src, server_src,
  dependencies:          common_deps + server_deps,
  include_directories:   ['inc', 'q2proto/inc'],
  gnu_symbol_visibility: 'hidden',
  win_subsystem:         'console,6.0',
  link_args:             exe_link_args,
  c_args:                ['-DUSE_SERVER=1', engine_args],
  cpp_args:              ['-DUSE_SERVER=1', engine_args],
  install:               true,
  install_dir:           bindir,
)

shared_library('game' + cpu, game_src,
  name_prefix:           '',
  dependencies:          game_deps,
  include_directories:   'inc',
  gnu_symbol_visibility: 'hidden',
  link_args:             dll_link_args,
  install:               true,
  install_dir:           libdir / get_option('base-game'),
  override_options:      get_option('game-build-options'),
)

rr_game_subproject = subproject('rerelease-game')
rr_game_dll = rr_game_subproject.get_variable('game_dll')

prog_copy_game_dll = find_program('copy-game-dll.py')
custom_target(
    'copy_game_dll',
    build_by_default: true,
    input : rr_game_dll,
    output: '@PLAINNAME@',
    command : [prog_copy_game_dll, '@OUTPUT@', '@INPUT@'],
    install: true,
    install_dir: libdir / get_option('base-game'),
    install_tag: 'runtime',
)

config.set('REVISION',            meson.project_version().substring(1).split('~')[0].to_int())
config.set_quoted('VERSION',      meson.project_version())
config.set_quoted('CPUSTRING',    cpu)
config.set_quoted('BUILDSTRING',  sys)
config.set_quoted('BASEGAME',     get_option('base-game'))
config.set_quoted('DEFGAME',      get_option('default-game'))
if not win32
  config.set_quoted('DATADIR',    datadir)
  config.set_quoted('LIBDIR',     libdir)
  config.set_quoted('HOMEDIR',    homedir)
  if default_prefix != ''
    config.set_quoted('DEFAULT_PREFIX', default_prefix)
  endif
endif
config.set_quoted('R_TEXTURE_FORMATS', ' '.join(texture_formats))
config.set_quoted('VID_GEOMETRY', get_option('vid-geometry'))
config.set_quoted('VID_MODELIST', get_option('vid-modelist'))

config.set10('USE_AUTOREPLY',     get_option('auto-reply'))
config.set10('USE_DEBUG',         get_option('debug'))
config.set10('USE_FPS',           get_option('variable-fps'))
config.set10('USE_GLES',          get_option('opengl-es1'))
config.set10('USE_ICMP',          get_option('icmp-errors').require(win32 or cc.has_header('linux/errqueue.h')).allowed())
config.set10('USE_MD3',           get_option('md3'))
config.set10('USE_MD5',           get_option('md5'))
config.set10('USE_MEMORY_TRACES', get_option('memory-traces'))
config.set10('USE_PACKETDUP',     get_option('packetdup-hack'))
config.set10('USE_TGA',           get_option('tga'))
config.set10('USE_' + host_machine.endian().to_upper() + '_ENDIAN', true)

if host_machine.system() == 'darwin'
  config.set_quoted('DEFGLPROFILE', 'gl3.2')
elif get_option('opengl-es1')
  config.set_quoted('DEFGLPROFILE', 'es1')
else
  config.set_quoted('DEFGLPROFILE', '')
endif

# protocol extensions are always on
config.set10('USE_PROTOCOL_EXTENSIONS', true)

if not win32
  have_backtrace = cc.has_header('execinfo.h') and cc.has_function('backtrace')
  config.set10('HAVE_BACKTRACE',  have_backtrace)
endif
config.set10('HAVE_MALLOC_H', cc.has_header('malloc.h'))
# new game API flag is *always on* for engine,
# and can be enabled here for game as well
if get_option('game-new-api')
  config.set10('USE_NEW_GAME_API', true)
endif

configure_file(output: 'config.h', configuration: config)

if system_wide
  summary({'datadir': datadir, 'libdir': libdir, 'homedir': homedir}, section: 'Directories')
endif

summary({
  'anticheat-server'   : config.get('USE_AC_SERVER', '') != '',
  'auto-reply'         : config.get('USE_AUTOREPLY', 0) != 0,
  'avcodec'            : config.get('USE_AVCODEC', 0) != 0,
  'client-gtv'         : config.get('USE_CLIENT_GTV', 0) != 0,
  'client-ui'          : config.get('USE_UI', 0) != 0,
  'debug'              : config.get('USE_DEBUG', 0) != 0,
  'game-abi-hack'      : config.get('USE_GAME_ABI_HACK', 0) != 0,
  'game-new-api'       : config.get('USE_NEW_GAME_API', 0) != 0,
  'icmp-errors'        : config.get('USE_ICMP', 0) != 0,
  'libcurl'            : config.get('USE_CURL', 0) != 0,
  'libjpeg'            : config.get('USE_JPG', 0) != 0,
  'libpng'             : config.get('USE_PNG', 0) != 0,
  'md3'                : config.get('USE_MD3', 0) != 0,
  'md5'                : config.get('USE_MD5', 0) != 0,
  'memory-traces'      : config.get('USE_MEMORY_TRACES', 0) != 0,
  'mvd-client'         : config.get('USE_MVD_CLIENT', 0) != 0,
  'mvd-server'         : config.get('USE_MVD_SERVER', 0) != 0,
  'openal'             : config.get('USE_OPENAL', 0) != 0,
  'packetdup-hack'     : config.get('USE_PACKETDUP', 0) != 0,
  'save-games'         : config.get('USE_SAVEGAMES', 0) != 0,
  'sdl2'               : config.get('USE_SDL', '') != '',
  'software-sound'     : config.get('USE_SNDDMA', 0) != 0,
  'system-console'     : config.get('USE_SYSCON', 0) != 0,
  'tests'              : config.get('USE_TESTS', 0) != 0,
  'tga'                : config.get('USE_TGA', 0) != 0,
  'variable-fps'       : config.get('USE_FPS', 0) != 0,
  'wayland'            : config.get('USE_WAYLAND', 0) != 0,
  'windows-crash-dumps': config.get('USE_DBGHELP', 0) != 0,
  'windows-egl'        : config.get('USE_WIN32EGL', 0) != 0,
  'windows-service'    : config.get('USE_WINSVC', '') != '',
  'x11'                : config.get('USE_X11', 0) != 0,
  'zlib'               : config.get('USE_ZLIB', 0) != 0,
  }, section: 'Features', bool_yn: true)
