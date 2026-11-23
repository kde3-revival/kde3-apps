# kde3-apps

kde3-apps holds the applications of the KDE3 revival desktop that are not
part of kdelibs, kdebase or kdepim.  They live in one source tree with one
CMake build, and each application has its own build option, so that every
application can be built and packaged on its own (one package per
application).

The applications come from the KDE 3.5.10 modules kdeutils and kdegraphics.
This repository merges the kde3-kdeutils and kde3-kdegraphics repositories
with their full history: every application keeps its directory, and
`git log` and `git blame` show its history back to the pristine 3.5.10
import.

## Applications

From KDE 3.5.10 kdeutils:

| Directory     | Application    | What it is                                                      |
|---------------|----------------|-----------------------------------------------------------------|
| `ark/`        | Ark            | Archive manager (tar, zip, 7-Zip, RAR, xz, zstd, lz4, ...), also a KPart |
| `kcalc/`      | KCalc          | Scientific calculator with arbitrary precision (GMP)           |
| `kcharselect/`| KCharSelect    | Picks special characters from any font and copies them to the clipboard |
| `kdf/`        | KDiskFree      | Free disk space viewer, with KwikDisk (tray) and a KInfoCenter module |
| `khexedit/`   | KHexEdit       | Binary file editor, with the KHexEdit2 widget library and KPart |
| `ktimer/`     | KTimer         | Runs programs after a countdown                                  |
| `kwallet/`    | KWalletManager | Manages the wallets of KWallet, with its Control Center module   |

From KDE 3.5.10 kdegraphics:

| Directory     | Application    | What it is                                                      |
|---------------|----------------|-----------------------------------------------------------------|
| `kruler/`     | KRuler         | Screen ruler in pixels, to measure distances on the screen      |
| `ksnapshot/`  | KSnapshot      | Screen capture: full screen, window or region                   |
| `kview/`      | KView          | Image viewer, also an embeddable KPart, with plugins            |

From KDE 3.5.10 kdetoys:

| Directory     | Application    | What it is                                                      |
|---------------|----------------|-----------------------------------------------------------------|
| `kweather/`   | KWeather       | Weather applet of the panel and DCOP weather service, forecasts from Open-Meteo |

The DCOP service of KWeather names a location `latitude,longitude,model`
instead of the METAR station code of KDE 3.5; see its handbook.

New in the KDE3 revival:

| Directory     | Application    | What it is                                                      |
|---------------|----------------|-----------------------------------------------------------------|
| `kwhatsapp/`  | KWhatsApp      | WhatsApp Web (WhatsApp's own web client) as a KDE application: its own window and login, a tray icon with the unread count, notifications through KNotify, shown as Kopete's balloon at the tray icon with the sender's picture; WPE WebKit through kde3-wpepart's KWPEPart (app mode) |

The handbook of each application is in `doc/<application>/`
(`doc/kinfocenter/blockdevices/` belongs to KDiskFree).

## Building

Dependencies:

- CMake 3.20 or newer, a C and C++ compiler (GCC), Ninja or make
- kde3-qt and kde3-kdelibs, installed; kdelibs is found through its CMake
  package `KDE3Libs`, which also sets the install prefix and layout
  (`/opt/kde3`), the compiler flags and the RUNPATH
- X11 with libXext (KSnapshot uses the X Shape extension)
- GMP (KCalc)

At run time Ark calls the command line archivers it finds (tar, gzip,
bzip2, xz, zstd, lz4, zip/unzip, 7z/7zz, rar/unrar, lha, ...), and KDiskFree
mounts and unmounts through `udisksctl` when it is installed.

Build and install:

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
          -DCMAKE_PREFIX_PATH=/opt/kde3
    cmake --build build
    cmake --install build          # or DESTDIR=... for a package

Options (all `ON` by default):

| Option                       | Builds                                                    |
|------------------------------|-----------------------------------------------------------|
| `KDE3_APPS_ARK`              | Ark                                                       |
| `KDE3_APPS_KCALC`            | KCalc (needs GMP; the configuration stops if it is missing) |
| `KDE3_APPS_KCHARSELECT`      | KCharSelect                                               |
| `KDE3_APPS_KDF`              | KDiskFree, KwikDisk and the KInfoCenter module            |
| `KDE3_APPS_KHEXEDIT`         | KHexEdit                                                  |
| `KDE3_APPS_KTIMER`           | KTimer                                                    |
| `KDE3_APPS_KWALLET`          | KWalletManager                                            |
| `KDE3_APPS_KVIEW`            | KView                                                     |
| `KDE3_APPS_KRULER`           | KRuler                                                    |
| `KDE3_APPS_KSNAPSHOT`        | KSnapshot                                                 |
| `KDE3_APPS_KSNAPSHOT_XSHAPE` | KSnapshot cuts non-rectangular windows to their shape (X Shape extension; the configuration stops if it is missing) |
| `KDE3_APPS_KWHATSAPP`        | KWhatsApp (at run time: kde3-wpepart with its app mode)    |

The handbook of an application is built with it.  To build a single
application, switch the others off, for example KCalc only:

    cmake -S . -B build-kcalc -G Ninja -DCMAKE_PREFIX_PATH=/opt/kde3 \
          -DKDE3_APPS_ARK=OFF -DKDE3_APPS_KCHARSELECT=OFF -DKDE3_APPS_KDF=OFF \
          -DKDE3_APPS_KHEXEDIT=OFF -DKDE3_APPS_KTIMER=OFF -DKDE3_APPS_KWALLET=OFF \
          -DKDE3_APPS_KVIEW=OFF -DKDE3_APPS_KRULER=OFF -DKDE3_APPS_KSNAPSHOT=OFF

## Other kde3-revival repositories

- [kde3-qt](https://github.com/kde3-revival/kde3-qt): Qt 3.3.8b, the base
  of everything below
- [kde3-kdelibs](https://github.com/kde3-revival/kde3-kdelibs): the KDE 3.5.10
  libraries; kde3-apps builds against its CMake package
- [kde3-kdebase](https://github.com/kde3-revival/kde3-kdebase): the desktop
  (kicker, kwin, konqueror, kcontrol, konsole, ...)
- [kde3-kdepim](https://github.com/kde3-revival/kde3-kdepim): KMail and the
  libraries it needs
- [kde3-wpepart](https://github.com/kde3-revival/kde3-wpepart): KWPEPart, a
  KPart that shows web pages with WPE WebKit, for Konqueror
- [kde3-packaging](https://github.com/kde3-revival/kde3-packaging):
  distribution packages (Arch Linux, Fedora) of the repositories above

kde3-apps replaces the separate kde3-kdeutils and kde3-kdegraphics
repositories; their histories are part of this one.

## Licences

As stated in the source files:

- The applications are under the GNU General Public License (GPL), version 2
  or (at your option) any later version: Ark, KCalc, KDiskFree, KHexEdit,
  KTimer, KWalletManager, KRuler.  Most files of KView say GPL version 2
  only; a few in `kview/kviewviewer/` say version 2 or later.  KCharSelect
  and some single files have no licence header; their About dialogs say GPL.
- KWhatsApp: GPL version 2 or later.  Its icon is WhatsApp's glyph (a
  trademark of WhatsApp), traced to SVG: see `kwhatsapp/ICONS`.  KWhatsApp is not made or endorsed by
  WhatsApp.
- KSnapshot: `main.cpp`, `regiongrabber.*` and `windowgrabber.*` are GPL
  version 2 or later; `ksnapshot.cpp`, `quickshot.cpp` and `quickshot.h`
  say "Released under the LGPL" without a version; the other files have no
  licence header.  Its About dialog says GPL.
- Libraries under the GNU Library General Public License (LGPL):
  `kcalc/knumber/` (version 2 or later), `khexedit/lib/` and
  `khexedit/parts/` (the KHexEdit2 widget library and its KPart, version 2),
  `kview/kimageviewer/` (the KImageViewer interfaces, version 2).
- The handbooks in `doc/` are under the GNU Free Documentation License,
  version 1.1 or any later version (their `&FDLNotice;`).

`COPYING` holds the GPL version 2, `COPYING-DOCS` the GFDL 1.2.

## Planned

- A PDF viewer: the KPDF interface on poppler-glib.
- A weather applet: KWeather revived on Open-Meteo (ECMWF forecasts).
