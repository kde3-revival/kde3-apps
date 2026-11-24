#!/bin/bash
# run-host.sh APPS_BUILD WPE_BUILD WORK: kwhatsapp's test (inner.sh, the
# local phase) on this machine, in a bwrap sandbox: no network but its
# own loopback, /tmp, /var/tmp, $HOME and /run empty (none of the user's
# KDE, D-Bus, X or WhatsApp session, no system service's socket), its own
# X server (:55) and D-Bus.  kwhatsapp (APPS_BUILD, a kde3-apps build) and the part
# (WPE_BUILD, a kde3-webpart build) are installed into WORK/build/inst
# (DESTDIR), the KDE of the machine (/opt/kde3, read-only) under them.
#   TOOLS  a directory with bin/ and lib/ of what the machine lacks
#          (xdotool, xwininfo, Xvfb: e.g. copied from the test image)
set -eu
AB=$(cd "$1" && pwd); PB=$(cd "$2" && pwd); mkdir -p "$3"; W=$(cd "$3" && pwd)
T=$(cd "$(dirname "$0")" && pwd)
KDE=$(readlink -f /opt/kde3)
QT=$(dirname "$(dirname "$(readlink -f "$(ldd "$KDE/lib/libkdecore.so.4" | awk '/libkde3-qt|libqt-mt/ {print $3; exit}')")")")
TOOLS=${TOOLS:-}
B=$W/build; O=$W/out-local
rm -rf "$B/inst" "$O"; mkdir -p "$B/inst" "$O"
DESTDIR=$B/inst ninja -C "$AB" install > /dev/null
DESTDIR=$B/inst ninja -C "$PB" install > /dev/null
qtlib=$(ls "$QT"/lib/libkde3-qt.so "$QT"/lib/libqt-mt.so 2> /dev/null | head -1)
qtname=$(basename "$qtlib" .so); qtname=${qtname#lib}
g++ -O1 -o "$B/dragsource" "$T/dragsource.cpp" -I"$QT/include" -L"$QT/lib" -Wl,-rpath,"$QT/lib" -l"$qtname"
gcc -O1 -o "$B/netclose" "$T/netclose.c" -lX11
args=(--ro-bind / / --dev /dev --tmpfs /dev/shm --proc /proc
      --tmpfs /tmp --tmpfs /var/tmp --tmpfs "$HOME" --tmpfs /run
      --ro-bind "$KDE" "$KDE" --ro-bind "$QT" "$QT" --ro-bind "$T" "$T" --bind "$W" "$W")
[ -n "$TOOLS" ] && args+=(--ro-bind "$(readlink -f "$TOOLS")" "$(readlink -f "$TOOLS")")
TL=${TOOLS:+$(readlink -f "$TOOLS")}
rc=0
nice bwrap "${args[@]}" --unshare-pid --unshare-net --unshare-ipc --die-with-parent --new-session \
  --unsetenv DISPLAY --unsetenv WAYLAND_DISPLAY --unsetenv DBUS_SESSION_BUS_ADDRESS --unsetenv SESSION_MANAGER \
  --unsetenv XAUTHORITY --unsetenv KDEHOME --unsetenv KDEDIRS --unsetenv LD_LIBRARY_PATH \
  --setenv KWT_TESTS "$T" --setenv KWT_BUILD "$B" --setenv KWT_RESULTS "$O" --setenv KWT_TOOLS "${TL:+$TL/bin}" \
  --setenv LD_LIBRARY_PATH "${TL:+$TL/lib}" --setenv COMPOSITING "${COMPOSITING:-true}" --setenv PHASES local \
  /usr/bin/dbus-run-session -- /bin/bash "$T/inner.sh" || rc=$?
cat "$O/summary.txt"
exit $rc
