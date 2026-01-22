project('worr', 'c', 'cpp',
  license: 'GPL-2.0-or-later',
  version: run_command(find_program('python3'), 'version.py', '--version', check: true).stdout().strip(),
  meson_version: '>= 0.60.0',
  default_options: [
    meson.version().version_compare('>=1.3.0') ? 'c_std=gnu11,c11' : 'c_std=gnu11',
    'cpp_std=c++17',
    'buildtype=debugoptimized',
  ],
)

common_src = [
  'src/common/bsp.c',
  'src/common/cmd.c',
  'src/common/cmodel.c',
  'src/common/common.c',
  'src/common/crc.c',
  'src/common/cvar.c',
  'src/common/error.c',
  'src/common/field.c',
  'src/common/fifo.c',
  'src/common/files.c',
  'src/common/game3_convert.c',
  'src/common/game3_pmove/new.c',
  'src/common/game3_pmove/old.c',
  'src/common/natsort.c',
  'src/common/hash_map.c',
  'src/common/loc.c',
  'src/common/math.c',
  'src/common/mdfour.c',
  'src/common/msg.c',
  'src/common/net/chan.c',
  'src/common/net/net.c',
  'src/common/pmove.c',
  'src/common/prompt.c',
  'src/common/q2proto_shared.c',
  'src/common/sizebuf.c',
  'src/common/steam.c',
  'src/common/utils.c',
  'src/common/zone.c',
  'src/common/features.h',
  'src/common/mapdb.c',
  'src/shared/shared.c',

  'src/common/q2proto-glue.c',
  'src/common/q2proto-packing.c',

  'inc/shared/atomic.h',
  'inc/shared/base85.h',
  'inc/shared/files.h',
  'inc/shared/game.h',
  'inc/shared/game3_shared.h',
  'inc/shared/list.h',
  'inc/shared/m_flash.h',
  'inc/shared/platform.h',
  'inc/shared/shared.h',

  'inc/common/natsort.h',
  'inc/common/async.h',
  'inc/common/bsp.h',
  'inc/common/cmd.h',
  'inc/common/cmodel.h',
  'inc/common/common.h',
  'inc/common/cvar.h',
  'inc/common/error.h',
  'inc/common/field.h',
  'inc/common/fifo.h',
  'inc/common/files.h',
  'inc/common/gamedll.h',
  'inc/common/hash_map.h',
  'inc/common/intreadwrite.h',
  'inc/common/loc.h',
  'inc/common/math.h',
  'inc/common/mdfour.h',
  'inc/common/msg.h',
  'inc/common/pmove.h',
  'inc/common/prompt.h',
  'inc/common/protocol.h',
  'inc/common/sizebuf.h',
  'inc/common/steam.h',
  'inc/common/tests.h',
  'inc/common/utils.h',
  'inc/common/zone.h',

  'inc/common/net/chan.h',
  'inc/common/net/net.h',

  'inc/format/bsp.h',
  'inc/format/md2.h',
  'inc/format/md3.h',
  'inc/format/pak.h',
  'inc/format/pcx.h',
  'inc/format/sp2.h',
  'inc/format/wal.h',

  'inc/system/hunk.h',
  'inc/system/pthread.h',
  'inc/system/system.h',
]

q2proto_src = [
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
]

client_src = [
  'src/client/ascii.cpp',
  'src/client/cgame.cpp',
  'src/client/cgame_classic.cpp',
  'src/client/console.cpp',
  'src/client/demo.cpp',
  'src/client/download.cpp',
  'src/client/effects.cpp',
  'src/client/entities.cpp',
  'src/client/font.cpp',
  'src/client/ui_font.cpp',
  'src/client/input.cpp',
  'src/client/keys.cpp',
  'src/client/locs.cpp',
  'src/client/main.cpp',
  'src/client/newfx.cpp',
  'src/client/parse.cpp',
  'src/client/precache.cpp',
  'src/client/predict.cpp',
  'src/client/renderer.cpp',
  'src/client/screen.cpp',
  'src/client/sound/main.cpp',
  'src/client/sound/mem.cpp',
  'src/client/sound/qal.h',
  'src/client/sound/sound.h',
  'src/client/tent.cpp',
  'src/client/ui_bridge.cpp',
  'src/client/view.cpp',
  'src/client/client.h',
  'src/client/cgame_classic.h',
  'src/common/async.c',
  'src/common/gamedll.c',
  'src/server/commands.c',
  'src/server/entities.c',
  'src/server/game.c',
  'src/server/game3_proxy/game3_proxy.c',
  'src/server/init.c',
  'src/server/main.c',
  'src/server/send.c',
  'src/server/user.c',
  'src/server/nav.c',
  'src/server/world.c',
  'src/server/server.h',
  'src/shared/base85.c',
  'src/shared/m_flash.c',

  'inc/server/server.h',

  'inc/client/client.h',
  'inc/client/input.h',
  'inc/client/keys.h',
  'inc/client/ui.h',
  'inc/client/video.h',

  'inc/client/sound/dma.h',
  'inc/client/sound/sound.h',
]

ui_src = [
  'src/game/cgame/ui/ui_cgame_access.cpp',
  'src/game/cgame/ui/ui_core.cpp',
  'src/game/cgame/ui/ui_menu.cpp',
  'src/game/cgame/ui/ui_hints.cpp',
  'src/game/cgame/ui/ui_widgets.cpp',
  'src/game/cgame/ui/ui_json.cpp',
  'src/game/cgame/ui/ui_conditions.cpp',
  'src/game/cgame/ui/ui_list.cpp',
  'src/game/cgame/ui/ui_page_demos.cpp',
  'src/game/cgame/ui/ui_page_servers.cpp',
  'src/game/cgame/ui/ui_page_player.cpp',
  'src/game/cgame/ui/ui_page_welcome.cpp',
  'src/game/cgame/ui/ui_mapdb.cpp',
  'src/game/cgame/ui/ui_playermodels.cpp',
]

renderer_src = [
  'src/rend_gl/draw.c',
  'src/rend_gl/hq2x.c',
  'src/rend_gl/images.c',
  'src/rend_gl/legacy.c',
  'src/rend_gl/main.c',
  'src/rend_gl/mesh.c',
  'src/rend_gl/models.c',
  'src/rend_gl/qgl.c',
  'src/rend_gl/shader.c',
  'src/rend_gl/sky.c',
  'src/rend_gl/state.c',
  'src/rend_gl/surf.c',
  'src/rend_gl/tess.c',
  'src/rend_gl/texture.c',
  'src/rend_gl/world.c',
  'src/rend_gl/debug.c',
  'src/rend_gl/debug_text.c',
  'src/rend_gl/arbfp.h',
  'src/rend_gl/gl.h',
  'src/rend_gl/images.h',
  'src/rend_gl/qgl.h',

  'inc/renderer/renderer.h',
]

renderer_vk_src = [
  'src/rend_vk/vkpt/asvgf.c',
  'src/rend_vk/vkpt/bloom.c',
  'src/rend_vk/vkpt/bsp_mesh.c',
  'src/rend_vk/vkpt/buddy_allocator.c',
  'src/rend_vk/vkpt/cameras.c',
  'src/rend_vk/vkpt/conversion.c',
  'src/rend_vk/vkpt/debug.c',
  'src/rend_vk/vkpt/device_memory_allocator.c',
  'src/rend_vk/vkpt/draw.c',
  'src/rend_vk/vkpt/fog.c',
  'src/rend_vk/vkpt/freecam.c',
  'src/rend_vk/vkpt/fsr.c',
  'src/rend_vk/vkpt/god_rays.c',
  'src/rend_vk/vkpt/main.c',
  'src/rend_vk/vkpt/material.c',
  'src/rend_vk/vkpt/matrix.c',
  'src/rend_vk/vkpt/mgpu.c',
  'src/rend_vk/vkpt/models.c',
  'src/rend_vk/vkpt/path_tracer.c',
  'src/rend_vk/vkpt/physical_sky.c',
  'src/rend_vk/vkpt/precomputed_sky.c',
  'src/rend_vk/vkpt/profiler.c',
  'src/rend_vk/vkpt/shadow_map.c',
  'src/rend_vk/vkpt/textures.c',
  'src/rend_vk/vkpt/tone_mapping.c',
  'src/rend_vk/vkpt/transparency.c',
  'src/rend_vk/vkpt/uniform_buffer.c',
  'src/rend_vk/vkpt/vertex_buffer.c',
  'src/rend_vk/vkpt/vk_util.c',

  'src/rend_vk/refresh/debug.c',
  'src/rend_vk/refresh/debug_text.c',
  'src/rend_vk/refresh/images.c',
  'src/rend_vk/refresh/model_iqm.c',
  'src/rend_vk/refresh/models.c',
  'src/rend_vk/refresh/refresh_ptrs.c',
  'src/rend_vk/refresh/stb/stb.c',

  'src/rend_vk/vkpt/vkpt.h',
  'src/rend_vk/vkpt/vk_util.h',
  'inc/renderer/renderer.h',
]

server_src = [
  'src/client/null.c',
  'src/common/gamedll.c',
  'src/server/commands.c',
  'src/server/entities.c',
  'src/server/game.c',
  'src/server/game3_proxy/game3_proxy.c',
  'src/server/init.c',
  'src/server/main.c',
  'src/server/send.c',
  'src/server/user.c',
  'src/server/nav.c',
  'src/server/world.c',
  'src/server/server.h',
  'src/shared/base85.c',

  'inc/server/server.h',
]

bgame_src = [
  'src/game/bgame/bg_local.hpp',
  'src/game/bgame/char_array_utils.hpp',
  'src/game/bgame/game.hpp',
  'src/game/bgame/logger.cpp',
  'src/game/bgame/logger.hpp',
  'src/game/bgame/map_validation.hpp',
  'src/game/bgame/m_flash.hpp',
  'src/game/bgame/q_std.hpp',
  'src/game/bgame/q_vec3.hpp',
  'src/game/bgame/string_compat.hpp',
  'src/game/bgame/version_autogen.hpp',
  'src/game/bgame/version.hpp',
  'src/game/bgame/weapon_pref_utils.hpp',
]

sgame_src = bgame_src + [
  'src/game/sgame/bots/bot_debug.cpp',
  'src/game/sgame/bots/bot_exports.cpp',
  'src/game/sgame/bots/bot_think.cpp',
  'src/game/sgame/bots/bot_utils.cpp',
  'src/game/sgame/client/client_session_service_impl.cpp',
  'src/game/sgame/client/client_stats_service.cpp',
  'src/game/sgame/commands/command_admin.cpp',
  'src/game/sgame/commands/command_cheats.cpp',
  'src/game/sgame/commands/command_client.cpp',
  'src/game/sgame/commands/command_system.cpp',
  'src/game/sgame/commands/command_voting.cpp',
  'src/game/sgame/gameplay/g_ai_new.cpp',
  'src/game/sgame/gameplay/g_ai.cpp',
  'src/game/sgame/gameplay/g_capture.cpp',
  'src/game/sgame/gameplay/g_client_cfg.cpp',
  'src/game/sgame/gameplay/g_clients.cpp',
  'src/game/sgame/gameplay/g_combat_heatmap.cpp',
  'src/game/sgame/gameplay/g_combat.cpp',
  'src/game/sgame/gameplay/g_domination.cpp',
  'src/game/sgame/gameplay/g_func.cpp',
  'src/game/sgame/gameplay/g_hud_blob.cpp',
  'src/game/sgame/gameplay/g_harvester.cpp',
  'src/game/sgame/gameplay/g_headhunters.cpp',
  'src/game/sgame/gameplay/g_horde.cpp',
  'src/game/sgame/gameplay/g_item_list.cpp',
  'src/game/sgame/gameplay/g_items.cpp',
  'src/game/sgame/gameplay/g_main.cpp',
  'src/game/sgame/gameplay/g_map_manager.cpp',
  'src/game/sgame/gameplay/g_misc.cpp',
  'src/game/sgame/gameplay/g_monster_spawn.cpp',
  'src/game/sgame/gameplay/g_monster.cpp',
  'src/game/sgame/gameplay/g_phys.cpp',
  'src/game/sgame/gameplay/g_proball.cpp',
  'src/game/sgame/gameplay/g_save.cpp',
  'src/game/sgame/gameplay/g_spawn_points.cpp',
  'src/game/sgame/gameplay/g_spawn.cpp',
  'src/game/sgame/gameplay/g_spectator.cpp',
  'src/game/sgame/gameplay/g_statusbar.cpp',
  'src/game/sgame/gameplay/g_svcmds.cpp',
  'src/game/sgame/gameplay/g_target.cpp',
  'src/game/sgame/gameplay/g_teamplay.cpp',
  'src/game/sgame/gameplay/g_trigger.cpp',
  'src/game/sgame/gameplay/g_turret.cpp',
  'src/game/sgame/gameplay/g_utilities.cpp',
  'src/game/sgame/gameplay/g_weapon.cpp',
  'src/game/sgame/match/match_logging.cpp',
  'src/game/sgame/match/match_state.cpp',
  'src/game/sgame/match/tournament.cpp',
  'src/game/sgame/menu/menu_page_admin_commands.cpp',
  'src/game/sgame/menu/menu_page_admin.cpp',
  'src/game/sgame/menu/menu_page_callvote.cpp',
  'src/game/sgame/menu/menu_page_forfeit.cpp',
  'src/game/sgame/menu/menu_page_hostinfo.cpp',
  'src/game/sgame/menu/menu_page_mapselector.cpp',
  'src/game/sgame/menu/menu_page_matchinfo.cpp',
  'src/game/sgame/menu/menu_page_matchstats.cpp',
  'src/game/sgame/menu/menu_page_mymap.cpp',
  'src/game/sgame/menu/menu_page_setup.cpp',
  'src/game/sgame/menu/menu_page_tournament.cpp',
  'src/game/sgame/menu/menu_page_voting.cpp',
  'src/game/sgame/menu/menu_page_welcome.cpp',
  'src/game/sgame/menu/menu_ui_helpers.cpp',
  'src/game/sgame/menu/menu_ui_list.cpp',
  'src/game/sgame/monsters/m_actor.cpp',
  'src/game/sgame/monsters/m_arachnid.cpp',
  'src/game/sgame/monsters/m_berserk.cpp',
  'src/game/sgame/monsters/m_brain.cpp',
  'src/game/sgame/monsters/m_carrier.cpp',
  'src/game/sgame/monsters/m_centroid.cpp',
  'src/game/sgame/monsters/m_chick.cpp',
  'src/game/sgame/monsters/m_chthon.cpp',
  'src/game/sgame/monsters/m_dog.cpp',
  'src/game/sgame/monsters/m_dragon.cpp',
  'src/game/sgame/monsters/m_eel.cpp',
  'src/game/sgame/monsters/m_enforcer.cpp',
  'src/game/sgame/monsters/m_fiend.cpp',
  'src/game/sgame/monsters/m_fixbot.cpp',
  'src/game/sgame/monsters/m_float.cpp',
  'src/game/sgame/monsters/m_flyer.cpp',
  'src/game/sgame/monsters/m_gekk.cpp',
  'src/game/sgame/monsters/m_gladiator.cpp',
  'src/game/sgame/monsters/m_grunt.cpp',
  'src/game/sgame/monsters/m_guardian.cpp',
  'src/game/sgame/monsters/m_guncmdr.cpp',
  'src/game/sgame/monsters/m_gunner.cpp',
  'src/game/sgame/monsters/m_hellknight.cpp',
  'src/game/sgame/monsters/m_hornet.cpp',
  'src/game/sgame/monsters/m_hover.cpp',
  'src/game/sgame/monsters/m_infantry.cpp',
  'src/game/sgame/monsters/m_insane.cpp',
  'src/game/sgame/monsters/m_jorg.cpp',
  'src/game/sgame/monsters/m_knight.cpp',
  'src/game/sgame/monsters/m_makron.cpp',
  'src/game/sgame/monsters/m_medic.cpp',
  'src/game/sgame/monsters/m_move.cpp',
  'src/game/sgame/monsters/m_mutant.cpp',
  'src/game/sgame/monsters/m_ogre.cpp',
  'src/game/sgame/monsters/m_overlord.cpp',
  'src/game/sgame/monsters/m_parasite.cpp',
  'src/game/sgame/monsters/m_rotfish.cpp',
  'src/game/sgame/monsters/m_scrag.cpp',
  'src/game/sgame/monsters/m_shambler.cpp',
  'src/game/sgame/monsters/m_shark.cpp',
  'src/game/sgame/monsters/m_shub.cpp',
  'src/game/sgame/monsters/m_soldier.cpp',
  'src/game/sgame/monsters/m_spawn.cpp',
  'src/game/sgame/monsters/m_spike.cpp',
  'src/game/sgame/monsters/m_stalker.cpp',
  'src/game/sgame/monsters/m_supertank.cpp',
  'src/game/sgame/monsters/m_sword.cpp',
  'src/game/sgame/monsters/m_tank.cpp',
  'src/game/sgame/monsters/m_turret.cpp',
  'src/game/sgame/monsters/m_vore.cpp',
  'src/game/sgame/monsters/m_widow.cpp',
  'src/game/sgame/monsters/m_widow2.cpp',
  'src/game/sgame/monsters/m_wrath.cpp',
  'src/game/sgame/monsters/m_wyvern.cpp',
  'src/game/sgame/monsters/m_zombie.cpp',
  'src/game/sgame/monsters/q1_support.cpp',
  'src/game/sgame/player/p_client.cpp',
  'src/game/sgame/player/p_hud_main.cpp',
  'src/game/sgame/player/p_hud_scoreboard.cpp',
  'src/game/sgame/player/p_move.cpp',
  'src/game/sgame/player/p_trail.cpp',
  'src/game/sgame/player/p_view.cpp',
  'src/game/sgame/player/p_weapon.cpp',
  'src/game/sgame/q_std.cpp',
]

cgame_src = bgame_src + [
  'src/game/sgame/player/p_move.cpp',
  'src/game/cgame/cg_local.h',
  'src/game/cgame/cg_main.cpp',
  'src/game/cgame/cg_draw.cpp',
  'src/game/cgame/cg_ui_export.cpp',
  'src/game/cgame/cg_ui_sys.cpp',
  'src/game/cgame/cg_fmt.cpp',
  'src/game/cgame/cg_parse.cpp',
  'src/game/cgame/cg_std.cpp',
  'src/game/cgame/cg_wheel.cpp',
]

updater_src = []
updater_deps = []

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

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

common_args = ['-DHAVE_CONFIG_H', '-DQ2PROTO_CONFIG_H="common/q2proto_config.h"']
if win32
  common_args += '-D_USE_MATH_DEFINES'
else
  common_args += '-D_GNU_SOURCE'
endif

engine_args = []
if win32
  engine_args += '-D_WIN32_WINNT=0x0601'
endif
engine_args += ['-DREF_GL=1', '-DREF_VKPT=2']

common_link_args = []
exe_link_args = []
dll_link_args = []
win_subsystem_client = 'windows'
win_subsystem_server = 'console'

if cc.get_argument_syntax() == 'gcc'
  if x86
    add_global_arguments('-msse2', '-mfpmath=sse', language: 'c')
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
  engine_args += cc.get_supported_arguments(['-Wmissing-prototypes'])

  if win32
    common_args += '-D__USE_MINGW_ANSI_STDIO=1'
    common_args += '-D_CRT_SECURE_NO_WARNINGS'
    common_args += '-D_CRT_NONSTDC_NO_WARNINGS'
    if cc.get_id() == 'clang'
      common_link_args += '-fuse-ld=lld'
    endif
    linker_id = cc.get_linker_id()
    if linker_id == 'link' or linker_id == 'lld-link'
      common_link_args += ['-Wl,/NXCOMPAT', '-Wl,/DYNAMICBASE']
      if cpu == 'x86_64'
        exe_link_args += ['-Wl,/HIGHENTROPYVA', '-Wl,/BASE:0x140000000']
        dll_link_args += ['-Wl,/HIGHENTROPYVA', '-Wl,/BASE:0x180000000']
      endif
    else
      common_link_args += '-Wl,--nxcompat,--dynamicbase'
      if cpu == 'x86_64'
        exe_link_args += '-Wl,--high-entropy-va,--image-base=0x140000000'
        dll_link_args += '-Wl,--high-entropy-va,--image-base=0x180000000'
      endif
      dll_link_args += '-static-libgcc'
    endif
  endif
elif cc.get_id() == 'msvc'
  common_args += ['/wd4146', '/wd4244', '/wd4305', '/D_CRT_SECURE_NO_WARNINGS', '/D_CRT_NONSTDC_NO_WARNINGS', '/wd4267', '/wd4018']
endif

client_cpp_args = [
  '-DHAVE_CONFIG_H',
  '-DQ2PROTO_CONFIG_H="common/q2proto_config.h"',
  '-DUSE_CLIENT=1',
  '-DUSE_REF=REF_GL',
]
if win32
  client_cpp_args += '-D_USE_MATH_DEFINES'
else
  client_cpp_args += '-D_GNU_SOURCE'
endif
client_cpp_args += engine_args

if cpp.get_argument_syntax() == 'gcc'
  if x86
    client_cpp_args += cpp.get_supported_arguments(['-msse2', '-mfpmath=sse'])
  endif

  client_cpp_warn_args = [
    '-Wformat-security',
    '-Wno-microsoft-anon-tag',
    '-Wpointer-arith',
    '-fms-extensions',
    '-fno-math-errno',
    '-fno-trapping-math',
    '-fsigned-char',
  ]
  client_cpp_args += cpp.get_supported_arguments(client_cpp_warn_args)

  client_cpp_std_args = cpp.get_supported_arguments([
    '-std=gnu++2b',
    '-std=c++2b',
    '-std=gnu++23',
    '-std=c++23',
    '-std=gnu++20',
    '-std=c++20',
  ])
  if client_cpp_std_args.length() > 0
    client_cpp_args += client_cpp_std_args[0]
  endif

  if win32
    client_cpp_args += ['-D_CRT_SECURE_NO_WARNINGS', '-D_CRT_NONSTDC_NO_WARNINGS']
  endif
elif cpp.get_id() == 'msvc'
  client_cpp_args += ['/std:c++latest', '/D_CRT_SECURE_NO_WARNINGS', '/D_CRT_NONSTDC_NO_WARNINGS']
endif

game_cpp_args = [
  '-DHAVE_CONFIG_H',
  '-DNO_FMT_SOURCE',
]

game_c_args = [
  '-DHAVE_CONFIG_H',
]

game_link_args = []

if cpp.get_argument_syntax() == 'gcc'
  if x86
    game_cpp_args += cpp.get_supported_arguments(['-msse2', '-mfpmath=sse'])
  endif

  game_warn_args = [
    '-Wformat-security',
    '-Wno-class-memaccess',
    '-Wno-narrowing',
    '-Wno-sign-compare',
    '-Wpointer-arith',
    '-Wno-array-bounds',
  ]

  game_cpp_args += cpp.get_supported_arguments(game_warn_args)

  if win32
    game_cpp_args += ['-D_CRT_SECURE_NO_WARNINGS', '-D_CRT_NONSTDC_NO_WARNINGS']
    linker_id = cpp.get_linker_id()
    if linker_id == 'link' or linker_id == 'lld-link'
      game_link_args += ['-Wl,/NXCOMPAT', '-Wl,/DYNAMICBASE']
      if cpu == 'x86_64'
        game_link_args += ['-Wl,/HIGHENTROPYVA', '-Wl,/BASE:0x180000000']
      endif
    else
      game_link_args += [
        '-Wl,--nxcompat,--dynamicbase',
        '-static-libgcc',
        '-static-libstdc++',
      ]
      if cpu == 'x86_64'
        game_link_args += '-Wl,--high-entropy-va,--image-base=0x180000000'
      endif
    endif
  endif
elif cpp.get_id() == 'msvc'
  game_cpp_args += '/wd4244'

  d = get_option('buildtype') == 'debug' ? 'd' : ''
  game_link_args += f'/nodefaultlib:libucrt@d@.lib'
  game_link_args += f'/defaultlib:ucrt@d@.lib'
endif

if win32 and cc.get_argument_syntax() == 'msvc'
  win_subsystem_client += ',6.0'
  win_subsystem_server += ',6.0'
endif

add_project_arguments(common_args, language: 'c')
add_project_link_arguments(common_link_args, language: 'c')

config = configuration_data()

texture_formats = []

fallback_opt = ['default_library=static']

game_fallback_opt = fallback_opt + [
  'b_vscrt=static_from_buildtype',
  'buildtype=' + get_option('buildtype'),
]

fmt_dep = dependency('fmt', default_options: game_fallback_opt)
jsoncpp_dep = dependency('jsoncpp', default_options: game_fallback_opt)

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

curl = dependency('libcurl',
  version:         '>= 7.68.0',
  required:        get_option('libcurl'),
  default_options: fallback_opt,
)

if curl.found()
  client_src += 'src/client/http.cpp'
  config.set10('USE_CURL', true)
endif

sdl3 = dependency('sdl3',
  version:         '>= 3.4.0',
  required:        get_option('sdl3'),
  fallback:        ['sdl3', 'sdl3_dep'],
  default_options: fallback_opt,
)
if sdl3.found()
  client_src += 'src/unix/video/sdl.c'
  config.set('USE_SDL', 'USE_CLIENT')
endif

freetype_fallback_opt = fallback_opt + [
  'harfbuzz=disabled',
  'brotli=disabled',
  'bzip2=disabled',
  'png=disabled',
  'zlib=internal',
]
freetype2 = dependency('freetype2',
  required:        get_option('sdl3-ttf'),
  fallback:        ['freetype2', 'freetype_dep'],
  default_options: freetype_fallback_opt,
)

harfbuzz_fallback_opt = fallback_opt + [
  'cpp_std=c++14',
  'cpp_args=-D_CRT_SECURE_NO_WARNINGS',
  'glib=disabled',
  'gobject=disabled',
  'cairo=disabled',
  'chafa=disabled',
  'icu=disabled',
  'graphite2=disabled',
  'freetype=enabled',
  'fontations=disabled',
  'gdi=disabled',
  'directwrite=disabled',
  'coretext=disabled',
  'harfrust=disabled',
  'kbts=disabled',
  'wasm=disabled',
  'tests=disabled',
  'utilities=disabled',
  'benchmark=disabled',
  'docs=disabled',
  'introspection=disabled',
]
harfbuzz = dependency('harfbuzz',
  required:        get_option('sdl3-ttf'),
  fallback:        ['harfbuzz', 'libharfbuzz_dep'],
  default_options: harfbuzz_fallback_opt,
)

sdl3_ttf = dependency('sdl3_ttf',
  required:        get_option('sdl3-ttf'),
  fallback:        ['sdl3_ttf', 'sdl3_ttf_dep'],
  default_options: fallback_opt,
)
config.set10('USE_SDL3_TTF', sdl3_ttf.found())
config.set10('USE_HARFBUZZ', harfbuzz.found() and sdl3_ttf.found())

khr_headers_dep = []
if win32
  khr_headers_dep = [subproject('khr-headers').get_variable('khr_headers_dep')]
endif

vulkan = dependency('vulkan', required: get_option('vulkan'))

common_deps = [zlib]
client_deps = [png, curl, sdl3]
if sdl3_ttf.found()
  client_deps += sdl3_ttf
endif
if harfbuzz.found()
  client_deps += harfbuzz
endif
server_deps = []
game_deps = [fmt_dep, jsoncpp_dep]

vkpt_shader_target = []
if vulkan.found() and cpu.endswith('64')
  vkpt_shader_sources = files(
    'src/rend_vk/vkpt/shader/animate_materials.comp',
    'src/rend_vk/vkpt/shader/god_rays_filter.comp',
    'src/rend_vk/vkpt/shader/god_rays.comp',
    'src/rend_vk/vkpt/shader/bloom_composite.comp',
    'src/rend_vk/vkpt/shader/bloom_blur.comp',
    'src/rend_vk/vkpt/shader/bloom_downscale.comp',
    'src/rend_vk/vkpt/shader/compositing.comp',
    'src/rend_vk/vkpt/shader/checkerboard_interleave.comp',
    'src/rend_vk/vkpt/shader/asvgf_atrous.comp',
    'src/rend_vk/vkpt/shader/asvgf_gradient_atrous.comp',
    'src/rend_vk/vkpt/shader/asvgf_gradient_img.comp',
    'src/rend_vk/vkpt/shader/asvgf_gradient_reproject.comp',
    'src/rend_vk/vkpt/shader/asvgf_lf.comp',
    'src/rend_vk/vkpt/shader/asvgf_taau.comp',
    'src/rend_vk/vkpt/shader/asvgf_temporal.comp',
    'src/rend_vk/vkpt/shader/instance_geometry.comp',
    'src/rend_vk/vkpt/shader/normalize_normal_map.comp',
    'src/rend_vk/vkpt/shader/tone_mapping_histogram.comp',
    'src/rend_vk/vkpt/shader/tone_mapping_curve.comp',
    'src/rend_vk/vkpt/shader/tone_mapping_apply.comp',
    'src/rend_vk/vkpt/shader/physical_sky.comp',
    'src/rend_vk/vkpt/shader/physical_sky_space.comp',
    'src/rend_vk/vkpt/shader/shadow_map.vert',
    'src/rend_vk/vkpt/shader/sky_buffer_resolve.comp',
    'src/rend_vk/vkpt/shader/stretch_pic.frag',
    'src/rend_vk/vkpt/shader/stretch_pic.vert',
    'src/rend_vk/vkpt/shader/final_blit.frag',
    'src/rend_vk/vkpt/shader/final_blit.vert',
    'src/rend_vk/vkpt/shader/fsr_easu_fp16.comp',
    'src/rend_vk/vkpt/shader/fsr_easu_fp32.comp',
    'src/rend_vk/vkpt/shader/fsr_rcas_fp16.comp',
    'src/rend_vk/vkpt/shader/fsr_rcas_fp32.comp',
    'src/rend_vk/vkpt/shader/debug_line.frag',
    'src/rend_vk/vkpt/shader/debug_line.vert',
  )

  vkpt_shader_rt_sources = files(
    'src/rend_vk/vkpt/shader/primary_rays.rgen',
    'src/rend_vk/vkpt/shader/direct_lighting.rgen',
    'src/rend_vk/vkpt/shader/indirect_lighting.rgen',
    'src/rend_vk/vkpt/shader/path_tracer.rchit',
    'src/rend_vk/vkpt/shader/path_tracer.rmiss',
    'src/rend_vk/vkpt/shader/path_tracer_masked.rahit',
    'src/rend_vk/vkpt/shader/path_tracer_particle.rahit',
    'src/rend_vk/vkpt/shader/path_tracer_sprite.rahit',
    'src/rend_vk/vkpt/shader/path_tracer_beam.rahit',
    'src/rend_vk/vkpt/shader/path_tracer_beam.rint',
    'src/rend_vk/vkpt/shader/path_tracer_explosion.rahit',
    'src/rend_vk/vkpt/shader/reflect_refract.rgen',
  )

  vkpt_shader_includes = files(
    'src/rend_vk/vkpt/shader/asvgf.glsl',
    'src/rend_vk/vkpt/shader/brdf.glsl',
    'src/rend_vk/vkpt/shader/fsr_easu.glsl',
    'src/rend_vk/vkpt/shader/fsr_rcas.glsl',
    'src/rend_vk/vkpt/shader/fsr_utils.glsl',
    'src/rend_vk/vkpt/shader/path_tracer_transparency.glsl',
    'src/rend_vk/vkpt/shader/precomputed_sky.glsl',
    'src/rend_vk/vkpt/shader/projection.glsl',
    'src/rend_vk/vkpt/shader/tone_mapping_utils.glsl',
    'src/rend_vk/vkpt/shader/utils.glsl',
    'src/rend_vk/vkpt/shader/water.glsl',
    'src/rend_vk/vkpt/shader/constants.h',
    'src/rend_vk/vkpt/shader/global_textures.h',
    'src/rend_vk/vkpt/shader/global_ubo.h',
    'src/rend_vk/vkpt/shader/god_rays_shared.h',
    'src/rend_vk/vkpt/shader/light_lists.h',
    'src/rend_vk/vkpt/shader/path_tracer_hit_shaders.h',
    'src/rend_vk/vkpt/shader/path_tracer_rgen.h',
    'src/rend_vk/vkpt/shader/path_tracer.h',
    'src/rend_vk/vkpt/shader/precomputed_sky_params.h',
    'src/rend_vk/vkpt/shader/shader_structs.h',
    'src/rend_vk/vkpt/shader/sky.h',
    'src/rend_vk/vkpt/shader/tiny_encryption_algorithm.h',
    'src/rend_vk/vkpt/shader/vertex_buffer.h',
    'src/rend_vk/vkpt/fsr/ffx_a.h',
    'src/rend_vk/vkpt/fsr/ffx_fsr1.h',
  )

  vkpt_shader_out_dir = meson.current_build_dir() / get_option('base-game') / 'shader_vkpt'
  prog_python = find_program('python3')
  prog_compile_vkpt_shaders = meson.current_source_dir() / 'tools' / 'compile_vkpt_shaders.py'

  vkpt_shader_target = custom_target(
    'vkpt_shaders',
    input: vkpt_shader_sources + vkpt_shader_rt_sources + vkpt_shader_includes,
    output: 'vkpt_shaders.stamp',
    command: [
      prog_python,
      prog_compile_vkpt_shaders,
      '--out-dir', vkpt_shader_out_dir,
      '--stamp', '@OUTPUT@'
    ],
    build_by_default: true,
  )
endif

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
  fallback:        ['openal-soft', 'openal_dep'],
  default_options: fallback_opt,
)
if openal.found()
  client_src += [ 'src/client/sound/al.cpp', 'src/client/sound/qal.cpp', 'inc/common/jsmn.h' ]
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
if avcodec.found() and avformat.found() and avutil.found() and swresample.found() and swscale.found()
  client_src += ['src/client/cin.cpp', 'src/client/sound/ogg.cpp']
  client_deps += [avcodec, avformat, avutil, swresample, swscale]
  config.set10('USE_AVCODEC', true)
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

prog_embed = find_program('embed.py')

# embed loc_english.txt for builtin fallback
loc_english_c = custom_target(
    'loc_english.txt.c',
    output : 'loc_english.txt.c',
    input : 'assets/localization/loc_english.txt',
    command : [prog_embed, '@INPUT@', '@OUTPUT@', 'res_loc_english'],
)
client_src += loc_english_c

if get_option('client-ui')

  # embed worr.json
  worr_json_c = custom_target(
      'worr.json.c',
      output : 'worr.json.c',
      input : 'src/game/cgame/ui/worr.json',
      command : [prog_embed, '@INPUT@', '@OUTPUT@', 'res_worr_json'],
  )
  client_src += worr_json_c

  config.set10('USE_UI', true)
endif

if get_option('software-sound').require(win32 or sdl3.found()).allowed()
  client_src += 'src/client/sound/dma.cpp'
  if sdl3.found()
    client_src += 'src/unix/sound/sdl.c'
  endif
  config.set10('USE_SNDDMA', true)
elif not openal.found()
  warning('Neither software sound nor OpenAL enabled, this is not supported')
endif

if get_option('anticheat-server')
  server_src += 'src/server/ac.c'
  config.set('USE_AC_SERVER', 'USE_SERVER')
endif

if get_option('mvd-server')
  common_src += [
    'src/server/mvd.c',
    'inc/server/mvd/client.h',
    'inc/server/mvd/protocol.h'
  ]
  config.set10('USE_MVD_SERVER', true)
endif

if get_option('mvd-client')
  common_src += [
    'src/server/mvd/client.c',
    'src/server/mvd/game.c',
    'src/server/mvd/parse.c'
  ]
  config.set10('USE_MVD_CLIENT', true)
endif

if get_option('save-games')
  common_src += 'src/server/save.c'
  config.set10('USE_SAVEGAMES', true)
endif

if get_option('system-console')
  if not win32
    common_src += 'src/unix/tty.c'
  endif
  config.set10('USE_SYSCON', true)
endif

if get_option('tests')
  common_src += 'src/common/tests.c'
  config.set10('USE_TESTS', true)
endif

renderer_engine_src = renderer_src
renderer_gl_lib = []
renderer_vk_lib = []

if get_option('external-renderers')
  renderer_engine_src = []

  renderer_deps = []
  if zlib.found()
    renderer_deps += zlib
  endif
  if png.found()
    renderer_deps += png
  endif
  if jpeg.found()
    renderer_deps += jpeg
  endif
  renderer_deps += khr_headers_dep

  renderer_c_args = ['-DUSE_CLIENT=1', '-DRENDERER_DLL'] + engine_args
  renderer_gl_c_args = renderer_c_args + ['-DUSE_REF=REF_GL']
  renderer_vk_c_args = renderer_c_args + ['-DUSE_REF=REF_VKPT', '-DRENDERER_VULKAN']

  renderer_gl_lib_src = renderer_src + ['src/renderer/renderer_api.c']
  renderer_gl_lib_name = meson.project_name() + '_opengl_' + cpu
  renderer_gl_lib = shared_library(renderer_gl_lib_name, renderer_gl_lib_src,
    dependencies:          renderer_deps,
    include_directories:   ['inc', 'q2proto/inc'],
    gnu_symbol_visibility: 'hidden',
    link_args:             dll_link_args,
    c_args:                renderer_gl_c_args,
    name_prefix:           '',
    install:               true,
    install_dir:           libdir,
  )

  if vulkan.found()
    renderer_vk_lib_src = renderer_vk_src + ['src/renderer/renderer_api.c']
    renderer_vk_lib_name = meson.project_name() + '_vulkan_' + cpu
    renderer_vk_lib = shared_library(renderer_vk_lib_name, renderer_vk_lib_src,
      dependencies:          renderer_deps + [vulkan],
      include_directories:   ['inc', 'q2proto/inc', 'src/rend_vk/refresh/stb', 'src/rend_vk/third_party'],
      gnu_symbol_visibility: 'hidden',
      link_args:             dll_link_args,
      c_args:                renderer_vk_c_args,
      name_prefix:           '',
      install:               true,
      install_dir:           libdir,
    )
  endif
endif

if get_option('game-abi-hack').require(x86 and cc.get_id() == 'gcc' and cc.has_argument('-mstackrealign')).allowed()
  config.set10('USE_GAME_ABI_HACK', true)
  engine_args += '-mstackrealign'
endif

q2proto_c_args = engine_args + []
if cc.get_argument_syntax() == 'gcc'
  q2proto_c_args += cc.get_supported_arguments(['-Wno-unused-function'])
endif

q2proto_lib = static_library('q2proto', q2proto_src,
  include_directories: ['inc', 'q2proto/inc'],
  c_args:              q2proto_c_args,
)

executable('worr', common_src, client_src, renderer_engine_src,
  dependencies:          common_deps + client_deps,
  include_directories:   ['inc', 'q2proto/inc'],
  gnu_symbol_visibility: 'hidden',
  win_subsystem:         win_subsystem_client,
  link_args:             exe_link_args,
  link_with:             q2proto_lib,
  c_args:                ['-DUSE_CLIENT=1', '-DUSE_REF=REF_GL', engine_args],
  cpp_args:              client_cpp_args,
  install:               true,
  install_dir:           bindir,
)

executable('worr.ded', common_src, server_src,
  dependencies:          common_deps + server_deps,
  include_directories:   ['inc', 'q2proto/inc'],
  gnu_symbol_visibility: 'hidden',
  win_subsystem:         win_subsystem_server,
  link_args:             exe_link_args,
  link_with:             q2proto_lib,
  c_args:                ['-DUSE_SERVER=1', engine_args],
  install:               true,
  install_dir:           bindir,
)

if win32 and get_option('bootstrapper')
  updater_src += [
    'src/updater/worr_updater.c',
    'src/updater/third_party/miniz.c',
    'src/updater/third_party/miniz_tdef.c',
    'src/updater/third_party/miniz_tinfl.c',
    'src/updater/third_party/miniz_zip.c',
  ]

  updater_c_args = [
    '-DMINIZ_NO_ARCHIVE_WRITING_APIS',
    '-DMINIZ_NO_ZLIB_COMPATIBLE_NAMES',
  ]
  if cc.get_argument_syntax() == 'gcc'
    updater_c_args += cc.get_supported_arguments(['-Wno-unused-function'])
  endif

  executable('worr_updater', updater_src,
    dependencies:        updater_deps,
    include_directories: ['inc', 'src/updater/third_party'],
    win_subsystem:       win_subsystem_client,
    link_args:           exe_link_args,
    c_args:              updater_c_args,
    install:             true,
    install_dir:         bindir,
  )
endif

sgame_inc = include_directories('src/game/sgame', 'src/game/bgame')
cgame_inc = include_directories('src/game/cgame', 'src/game/bgame')

cgame_defines = [
  '-DUSE_CLIENT=1',
  '-DUSE_REF=1',
  '-DUSE_UI=1',
  '-DRENDERER_DLL',
]

sgame_dll = shared_library('sgame' + cpu, sgame_src,
  name_prefix:           '',
  gnu_symbol_visibility: 'hidden',
  dependencies:          game_deps,
  include_directories:   sgame_inc,
  c_args:                game_c_args,
  cpp_args:              game_cpp_args,
  link_args:             dll_link_args + game_link_args,
  install:              true,
  install_dir:          libdir / get_option('base-game'),
  install_tag:          'runtime',
  override_options:      ['b_vscrt=static_from_buildtype', 'cpp_std=c++23'],
)

cgame_all_src = cgame_src + ui_src + [
  'src/common/field.c',
  'src/common/jsmn.c',
  'src/common/mapdb.c',
  'src/common/mdfour.c',
  'src/common/utf8.c',
  'src/shared/shared.c',
]

cgame_inc = [
  include_directories('inc', 'q2proto/inc'),
  cgame_inc,
]

cgame_dll = shared_library('cgame' + cpu, cgame_all_src,
  name_prefix:           '',
  gnu_symbol_visibility: 'hidden',
  dependencies:          game_deps,
  include_directories:   cgame_inc,
  c_args:                game_c_args + cgame_defines,
  cpp_args:              game_cpp_args + cgame_defines,
  link_args:             dll_link_args + game_link_args,
  install:              true,
  install_dir:          libdir / get_option('base-game'),
  install_tag:          'runtime',
  override_options:     ['b_vscrt=static_from_buildtype', 'cpp_std=c++23'],
)

prog_copy_game_dll = find_program('copy-game-dll.py')
custom_target(
    'copy_sgame_dll',
    build_by_default: true,
    input : sgame_dll,
    output: '@PLAINNAME@.stamp',
    command : [prog_copy_game_dll, '@OUTPUT@', '@INPUT@', '--copy-dir', meson.current_build_dir() / get_option('base-game'), '--stamp'],
)
custom_target(
    'copy_cgame_dll',
    build_by_default: true,
    input : cgame_dll,
    output: '@PLAINNAME@.stamp',
    command : [prog_copy_game_dll, '@OUTPUT@', '@INPUT@', '--copy-dir', meson.current_build_dir() / get_option('base-game'), '--stamp'],
)

version_prog = find_program('python3')
version_revision = run_command(version_prog, 'version.py', '--revision', check: true).stdout().strip()
version_semver = run_command(version_prog, 'version.py', '--semver', check: true).stdout().strip()
version_major = run_command(version_prog, 'version.py', '--major', check: true).stdout().strip()
version_minor = run_command(version_prog, 'version.py', '--minor', check: true).stdout().strip()
version_patch = run_command(version_prog, 'version.py', '--patch', check: true).stdout().strip()
version_prerelease = run_command(version_prog, 'version.py', '--prerelease', check: true).stdout().strip()

config.set('REVISION',            version_revision.to_int())
config.set('VERSION_MAJOR',       version_major.to_int())
config.set('VERSION_MINOR',       version_minor.to_int())
config.set('VERSION_PATCH',       version_patch.to_int())
config.set_quoted('VERSION',      meson.project_version())
config.set_quoted('VERSION_SEMVER', version_semver)
config.set_quoted('VERSION_PRERELEASE', version_prerelease)
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
config.set10('USE_VULKAN',        vulkan.found())
config.set10('USE_ICMP',          get_option('icmp-errors').require(win32 or cc.has_header('linux/errqueue.h')).allowed())
config.set10('USE_EXTERNAL_RENDERERS', get_option('external-renderers'))
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
  'sdl3'               : config.get('USE_SDL', '') != '',
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
