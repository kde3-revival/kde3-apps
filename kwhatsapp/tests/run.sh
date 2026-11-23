#!/bin/bash
# run.sh SRC_APPS SRC_WPE WORK [PHASES]: kwhatsapp's test in a podman
# container: builds kde3-wpepart (SRC_WPE, the app mode) and kwhatsapp
# (SRC_APPS) against the machine's /opt/kde3 into WORK/build, then runs
# inner.sh (PHASES: local, whatsapp; default local) with its results in
# WORK/out-PHASES (summary.txt, screenshots, logs, memory.txt).
#   IMAGE      default localhost/kwhatsapp-test (see Containerfile)
#   KDE3_PREFIX, QT3_PREFIX  mounted read-only (default /opt/kde3, /opt/kde3-qt)
# "local" runs without a network (--network=none); "whatsapp" needs one.
set -u
A=$(cd "$1" && pwd); P=$(cd "$2" && pwd); mkdir -p "$3"; W=$(cd "$3" && pwd)
PHASES=${4:-local}
IMAGE=${IMAGE:-localhost/kwhatsapp-test}
KDE3_PREFIX=${KDE3_PREFIX:-/opt/kde3}; QT3_PREFIX=${QT3_PREFIX:-/opt/kde3-qt}
mkdir -p "$W/build"
common=(--rm --security-opt label=disable --shm-size=512m
        -v "$A":/src/apps:ro -v "$P":/src/wpe:ro -v "$W/build":/build)
# KDE3_PREFIX=image: the image's own kde3 packages (a machine whose
# /opt/kde3 is not built for Fedora 44)
[ "$KDE3_PREFIX" = image ] || common+=(-v "$KDE3_PREFIX":/opt/kde3:ro -v "$QT3_PREFIX":/opt/kde3-qt:ro)
if [ ! -x "$W/build/inst/opt/kde3/bin/kwhatsapp" ] || [ -n "${REBUILD:-}" ]; then
  podman run "${common[@]}" --network=none "$IMAGE" /src/apps/kwhatsapp/tests/build.sh || exit 1
fi
OUT="$W/out-${PHASES// /-}"
rm -rf "$OUT"; mkdir -p "$OUT"
net=--network=none
case " $PHASES " in *" whatsapp "*) net= ;; esac
# the installed part of the machine (an older one) must not be loaded
extra=()
[ "$KDE3_PREFIX" != image ] && [ -e "$KDE3_PREFIX/lib/kde3/libkwpepart.so" ] && \
  extra=(-v "$W/build/inst/opt/kde3/lib/kde3/libkwpepart.so":/opt/kde3/lib/kde3/libkwpepart.so:ro)
nice podman run "${common[@]}" "${extra[@]}" $net -v "$OUT":/out -e PHASES="$PHASES" \
  --name "kwhatsapp-test-$$" "$IMAGE" /src/apps/kwhatsapp/tests/inner.sh
rc=$?
cat "$OUT/summary.txt"
exit $rc
