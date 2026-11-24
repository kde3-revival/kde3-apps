#!/bin/sh
# build.sh: inside the test container: kde3-wpepart (/src/wpe) and
# kwhatsapp (/src/apps, the other applications off) built against
# /opt/kde3, installed into /build/inst; the drop test's drag source,
# netclose (a window closed by the window manager).
set -e
cmake -G Ninja -S /src/wpe -B /build/wpe -DCMAKE_PREFIX_PATH=/opt/kde3 -DCMAKE_INSTALL_PREFIX=/opt/kde3 \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo -DKWPE_RENDER_ONLY_TEST=ON > /build/wpe-cmake.log
ninja -C /build/wpe > /build/wpe-build.log 2>&1 || { cat /build/wpe-build.log; exit 1; }
DESTDIR=/build/inst ninja -C /build/wpe install > /dev/null
off=
for a in ARK KCALC KCHARSELECT KDF KHEXEDIT KTIMER KWALLET KVIEW KRULER KSNAPSHOT KWEATHER KPDF; do off="$off -DKDE3_APPS_$a=OFF"; done
cmake -G Ninja -S /src/apps -B /build/apps -DCMAKE_PREFIX_PATH=/opt/kde3 -DCMAKE_BUILD_TYPE=RelWithDebInfo $off \
      > /build/apps-cmake.log
ninja -C /build/apps > /build/apps-build.log 2>&1 || { cat /build/apps-build.log; exit 1; }
DESTDIR=/build/inst ninja -C /build/apps install > /dev/null
qtlib=$(ls /opt/kde3-qt/lib*/libkde3-qt.so /opt/kde3-qt/lib*/libqt-mt.so 2> /dev/null | head -1)
qtname=$(basename "$qtlib" .so); qtname=${qtname#lib}
g++ -O1 -o /build/dragsource /src/apps/kwhatsapp/tests/dragsource.cpp -I/opt/kde3-qt/include \
    -L"$(dirname "$qtlib")" -Wl,-rpath,"$(dirname "$qtlib")" -l"$qtname"
gcc -O1 -o /build/netclose /src/apps/kwhatsapp/tests/netclose.c -lX11
echo "warnings: wpepart $(grep -c 'warning:' /build/wpe-build.log), kwhatsapp $(grep -c 'warning:' /build/apps-build.log)"
