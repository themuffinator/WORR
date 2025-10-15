#!/bin/sh
#
# Helper script to create or reconfigure a Meson build directory and optionally
# start a Meson development environment shell.
#
set -eu

usage() {
  cat <<USAGE
Usage: $0 [-b builddir] [-c command] [-n] [-- meson-setup-args...]

Options:
  -b builddir  Location of the Meson build directory (default: builddir)
  -c command   Command to execute via "sh -c" inside meson devenv
  -n           Do not start the Meson development environment after setup
  -h           Show this help message

All arguments after "--" are passed directly to "meson setup".
If no command is provided, an interactive shell is started inside meson devenv.
USAGE
}

builddir="builddir"
run_devenv=true
devenv_command=""

while getopts "b:c:nh" opt; do
  case "${opt}" in
    b)
      builddir="${OPTARG}"
      ;;
    c)
      devenv_command="${OPTARG}"
      ;;
    n)
      run_devenv=false
      ;;
    h)
      usage
      exit 0
      ;;
    \?)
      usage >&2
      exit 1
      ;;
  esac
done
shift $((OPTIND - 1))

if [ "${builddir}" = "" ]; then
  echo "error: build directory path cannot be empty" >&2
  exit 1
fi

if [ -d "${builddir}/meson-private" ]; then
  echo "Reconfiguring existing Meson build directory: ${builddir}"
  meson setup --reconfigure "${builddir}" "$@"
else
  echo "Creating Meson build directory: ${builddir}"
  meson setup "${builddir}" "$@"
fi

if ${run_devenv}; then
  if [ -n "${devenv_command}" ]; then
    echo "Starting Meson development environment in '${builddir}' (command: ${devenv_command})"
    meson devenv -C "${builddir}" -- "${SHELL:-/bin/sh}" -c "${devenv_command}"
  else
    echo "Starting interactive Meson development environment in '${builddir}'"
    meson devenv -C "${builddir}"
  fi
else
  cat <<INFO
Meson build directory '${builddir}' is ready.
Run 'meson compile -C ${builddir}' to build the project.
Run 'meson devenv -C ${builddir}' to enter the development environment.
INFO
fi
