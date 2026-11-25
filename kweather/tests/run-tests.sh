#!/bin/bash
# run-tests.sh BUILD [PNGDIR]: kweathertodaytest of the kde3-apps build
# BUILD (configured with -DKDE3_APPS_KWEATHER_TESTS=ON) in a bwrap
# sandbox: no network, /tmp, /var/tmp, $HOME and /run empty (none of the
# user's KDE, D-Bus or X), /opt/kde3 and Qt read-only.  With PNGDIR, also the
# pictures of the cases, on its own X server (Xvfb :57; XVFB names the
# program if it is not in PATH).
set -eu
B=$(cd "$1" && pwd)
T=$B/kweather/tests/kweathertodaytest
KDE=$(readlink -f /opt/kde3)
QT=$(dirname "$(dirname "$(readlink -f "$(ldd "$T" | awk '/libkde3-qt|libqt-mt/ {print $3; exit}')")")")
args=(--ro-bind / / --dev /dev --tmpfs /dev/shm --proc /proc
      --tmpfs /tmp --tmpfs /var/tmp --tmpfs "$HOME" --tmpfs /run
      --ro-bind "$KDE" "$KDE" --ro-bind "$QT" "$QT" --ro-bind "$B" "$B")
env=(--unsetenv DISPLAY --unsetenv WAYLAND_DISPLAY --unsetenv DBUS_SESSION_BUS_ADDRESS
     --unsetenv SESSION_MANAGER --unsetenv XAUTHORITY --unsetenv KDEHOME --unsetenv KDEDIRS
     --setenv LANG C.UTF-8 --setenv LC_ALL C.UTF-8 --setenv TZ UTC)
if [ $# -lt 2 ]; then
  exec nice bwrap "${args[@]}" --unshare-all --die-with-parent --new-session "${env[@]}" "$T"
fi
mkdir -p "$2"; P=$(cd "$2" && pwd)
X=${XVFB:-Xvfb}; X=$(command -v "$X")
args+=(--bind "$P" "$P" --ro-bind "$(dirname "$X")" "$(dirname "$X")")
exec nice bwrap "${args[@]}" --unshare-all --die-with-parent --new-session "${env[@]}" \
  /bin/bash -c '"$1" :57 -screen 0 800x600x24 -nolisten tcp > /tmp/xvfb.log 2>&1 & x=$!
    for i in $(seq 50); do [ -e /tmp/.X11-unix/X57 ] && break; sleep 0.1; done
    rc=0; DISPLAY=:57 "$2" --png "$3" || rc=$?; kill $x; exit $rc' sh "$X" "$T" "$P"
