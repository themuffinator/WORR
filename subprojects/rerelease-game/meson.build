project('quake2-rerelease-dll', 'cpp',
  license: 'GPL-2.0-or-later',
  version: run_command(find_program('python3'), 'version.py', check: true).stdout().strip(),
  default_options: [
    'cpp_std=c++latest',
    'buildtype=debugoptimized',
    'b_vscrt=static_from_buildtype',
  ],
)


src = [
  'rerelease/cg_local.h',
  'rerelease/cg_main.cpp',
  'rerelease/cg_screen.cpp',
  'rerelease/g_ai.cpp',
  'rerelease/g_chase.cpp',
  'rerelease/g_cmds.cpp',
  'rerelease/g_combat.cpp',
  'rerelease/g_func.cpp',
  'rerelease/g_items.cpp',
  'rerelease/g_local.h',
  'rerelease/g_main.cpp',
  'rerelease/g_misc.cpp',
  'rerelease/g_monster.cpp',
  'rerelease/g_phys.cpp',
  'rerelease/g_save.cpp',
  'rerelease/g_spawn.cpp',
  'rerelease/g_statusbar.h',
  'rerelease/g_svcmds.cpp',
  'rerelease/g_target.cpp',
  'rerelease/g_trigger.cpp',
  'rerelease/g_turret.cpp',
  'rerelease/g_utils.cpp',
  'rerelease/g_weapon.cpp',
  'rerelease/game.h',
  'rerelease/m_actor.cpp',
  'rerelease/m_actor.h',
  'rerelease/m_arachnid.cpp',
  'rerelease/m_arachnid.h',
  'rerelease/m_berserk.cpp',
  'rerelease/m_berserk.h',
  'rerelease/m_boss2.cpp',
  'rerelease/m_boss2.h',
  'rerelease/m_boss3.cpp',
  'rerelease/m_boss31.cpp',
  'rerelease/m_boss31.h',
  'rerelease/m_boss32.cpp',
  'rerelease/m_boss32.h',
  'rerelease/m_brain.cpp',
  'rerelease/m_brain.h',
  'rerelease/m_chick.cpp',
  'rerelease/m_chick.h',
  'rerelease/m_flash.h',
  'rerelease/m_flipper.cpp',
  'rerelease/m_flipper.h',
  'rerelease/m_float.cpp',
  'rerelease/m_float.h',
  'rerelease/m_flyer.cpp',
  'rerelease/m_flyer.h',
  'rerelease/m_gladiator.cpp',
  'rerelease/m_gladiator.h',
  'rerelease/m_guardian.cpp',
  'rerelease/m_guardian.h',
  'rerelease/m_guncmdr.cpp',
  'rerelease/m_gunner.cpp',
  'rerelease/m_gunner.h',
  'rerelease/m_hover.cpp',
  'rerelease/m_hover.h',
  'rerelease/m_infantry.cpp',
  'rerelease/m_infantry.h',
  'rerelease/m_insane.cpp',
  'rerelease/m_insane.h',
  'rerelease/m_medic.cpp',
  'rerelease/m_medic.h',
  'rerelease/m_move.cpp',
  'rerelease/m_mutant.cpp',
  'rerelease/m_mutant.h',
  'rerelease/m_parasite.cpp',
  'rerelease/m_parasite.h',
  'rerelease/m_player.h',
  'rerelease/m_rider.h',
  'rerelease/m_shambler.cpp',
  'rerelease/m_shambler.h',
  'rerelease/m_soldier.cpp',
  'rerelease/m_soldier.h',
  'rerelease/m_supertank.cpp',
  'rerelease/m_supertank.h',
  'rerelease/m_tank.cpp',
  'rerelease/m_tank.h',
  'rerelease/p_client.cpp',
  'rerelease/p_hud.cpp',
  'rerelease/p_move.cpp',
  'rerelease/p_trail.cpp',
  'rerelease/p_view.cpp',
  'rerelease/p_weapon.cpp',
  'rerelease/q_std.cpp',
  'rerelease/q_std.h',
  'rerelease/q_vec3.h',
]

src += [
  'rerelease/bots/bot_debug.cpp',
  'rerelease/bots/bot_debug.h',
  'rerelease/bots/bot_exports.cpp',
  'rerelease/bots/bot_exports.h',
  'rerelease/bots/bot_includes.h',
  'rerelease/bots/bot_think.cpp',
  'rerelease/bots/bot_think.h',
  'rerelease/bots/bot_utils.cpp',
  'rerelease/bots/bot_utils.h',
]

src += [
  'rerelease/ctf/g_ctf.cpp',
  'rerelease/ctf/g_ctf.h',
  'rerelease/ctf/p_ctf_menu.cpp',
  'rerelease/ctf/p_ctf_menu.h',
]

src += [
  'rerelease/rogue/g_rogue_combat.cpp',
  'rerelease/rogue/g_rogue_func.cpp',
  'rerelease/rogue/g_rogue_items.cpp',
  'rerelease/rogue/g_rogue_misc.cpp',
  'rerelease/rogue/g_rogue_monster.cpp',
  'rerelease/rogue/g_rogue_newai.cpp',
  'rerelease/rogue/g_rogue_newdm.cpp',
  'rerelease/rogue/g_rogue_newfnc.cpp',
  'rerelease/rogue/g_rogue_newtarg.cpp',
  'rerelease/rogue/g_rogue_newtrig.cpp',
  'rerelease/rogue/g_rogue_newweap.cpp',
  'rerelease/rogue/g_rogue_phys.cpp',
  'rerelease/rogue/g_rogue_spawn.cpp',
  'rerelease/rogue/g_rogue_sphere.cpp',
  'rerelease/rogue/g_rogue_utils.cpp',
  'rerelease/rogue/m_rogue_carrier.cpp',
  'rerelease/rogue/m_rogue_carrier.h',
  'rerelease/rogue/m_rogue_stalker.cpp',
  'rerelease/rogue/m_rogue_stalker.h',
  'rerelease/rogue/m_rogue_turret.cpp',
  'rerelease/rogue/m_rogue_turret.h',
  'rerelease/rogue/m_rogue_widow.cpp',
  'rerelease/rogue/m_rogue_widow.h',
  'rerelease/rogue/m_rogue_widow2.cpp',
  'rerelease/rogue/m_rogue_widow2.h',
  'rerelease/rogue/p_rogue_weapon.cpp',
  'rerelease/rogue/rogue_dm_ball.cpp',
  'rerelease/rogue/rogue_dm_tag.cpp',
]

src += [
  'rerelease/xatrix/g_xatrix_func.cpp',
  'rerelease/xatrix/g_xatrix_items.cpp',
  'rerelease/xatrix/g_xatrix_misc.cpp',
  'rerelease/xatrix/g_xatrix_monster.cpp',
  'rerelease/xatrix/g_xatrix_target.cpp',
  'rerelease/xatrix/g_xatrix_weapon.cpp',
  'rerelease/xatrix/m_xatrix_fixbot.cpp',
  'rerelease/xatrix/m_xatrix_fixbot.h',
  'rerelease/xatrix/m_xatrix_gekk.cpp',
  'rerelease/xatrix/m_xatrix_gekk.h',
  'rerelease/xatrix/p_xatrix_weapon.cpp',
]

cpp = meson.get_compiler('cpp')

win32 = host_machine.system() == 'windows'
x86 = host_machine.cpu_family() == 'x86'

cpuremap = {
  'x86': win32 ? 'x86' : 'i386',
  'x86_64': win32 ? 'x64' : 'x86_64',
  'aarch64': 'arm64',
}

cpu = host_machine.cpu_family()
if cpu in cpuremap
  cpu = cpuremap[cpu]
endif

args = [
  '-DHAVE_CONFIG_H',
  '-DNO_FMT_SOURCE',
]

link_args = []

if cpp.get_argument_syntax() == 'gcc'
  if not meson.is_subproject() and x86
    add_global_arguments('-msse2', '-mfpmath=sse', language: 'cpp')
  endif

  test_args = [
    '-Wformat-security',
    '-Wno-class-memaccess',
    '-Wno-narrowing',
    '-Wno-sign-compare',
    '-Wpointer-arith',
    '-Wno-array-bounds', # produces false positive on GCC 13
  ]

  args += cpp.get_supported_arguments(test_args)

  if win32
    link_args += [
      '-Wl,--nxcompat,--dynamicbase',
      '-static-libgcc',
      '-static-libstdc++',
    ]
    if cpu == 'x64'
      link_args += '-Wl,--high-entropy-va,--image-base=0x180000000'
    endif
  endif
elif cpp.get_id() == 'msvc'
  args += '/wd4244'

  # Always link dynamic UCRT
  d = get_option('buildtype') == 'debug' ? 'd' : ''
  link_args += f'/nodefaultlib:libucrt@d@.lib'
  link_args += f'/defaultlib:ucrt@d@.lib'
endif

add_project_arguments(args, language: 'cpp')
add_project_link_arguments(link_args, language: 'cpp')

fallback_opt = ['default_library=static', 'b_vscrt=static_from_buildtype', 'buildtype=' + get_option('buildtype'), ]

deps = [
  dependency('fmt', default_options: fallback_opt),
  dependency('jsoncpp', default_options: fallback_opt),
]

game_dll = shared_library('game_' + cpu, src,
  name_prefix:           '',
  gnu_symbol_visibility: 'hidden',
  dependencies:          deps,
)

config = configuration_data()
config.set('REVISION',         meson.project_version().substring(1).split('~')[0].to_int())
config.set_quoted('VERSION',   meson.project_version())
config.set_quoted('CPUSTRING', cpu)

configure_file(output: 'config.h', configuration: config)
