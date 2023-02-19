#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"/game

MANGOHUD="$(command -v mangohud || true)"
ARGS="-steam -game tf -insecure -novid -nojoy -nosteamcontroller -nohltv -particles 1 -noborder -particle_fallback 2 -dev -nobreakpad -console"

export LAUNCH_PREFIX="$HOME/.steam/bin32/steam-runtime/run.sh"

if [[ "$1" == "-d" ]]; then
	# shellcheck disable=SC2086
	# shellcheck disable=SC2068
	GAME_DEBUGGER=lldb ${LAUNCH_PREFIX} "$(pwd)"/hl2.sh -- -allowdebug ${ARGS} ${@:2}
elif [[ "$1" == "-b" ]]; then
	rm tf/cfg/config.cfg
	mkdir -p ../benchlogs
	# shellcheck disable=SC2086
	MANGOHUD_DLSYM=1 MANGOHUD_CONFIG="output_folder=$(pwd)/../benchlogs,toggle_logging=F12" \
		${LAUNCH_PREFIX} mangohud "$(pwd)"/hl2.sh \
		${ARGS} "+bind f12 demo_resume" "+fps_max 0" "+playdemo demos/${2}" "+demo_pause"
else
	# shellcheck disable=SC2086
	# shellcheck disable=SC2068
	${LAUNCH_PREFIX} ${MANGOHUD} "$(pwd)"/hl2.sh ${ARGS} $@
fi
