/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <qimage.h>
#include <qstringlist.h>

#include <kglobal.h>
#include <kiconloader.h>
#include <klocale.h>

#include "icons.h"

// The first of the names the icon theme has, in the given size
static QPixmap firstIcon(const QStringList &names, int size)
{
    KIconLoader *loader = KGlobal::iconLoader();
    for (QStringList::ConstIterator it = names.begin(); it != names.end(); ++it) {
        QPixmap pm = loader->loadIcon(*it, KIcon::NoGroup, size, KIcon::DefaultState, 0, true);
        if (!pm.isNull())
            return pm;
    }
    return QPixmap();
}

QPixmap KBluetoothIcons::trayIcon(State state, int size)
{
    // the freedesktop names first, for the themes that have them;
    // kbluetooth's own icons (in hicolor) are the fallback
    QStringList names;
    switch (state) {
    case Off:
        names << "bluetooth-disabled" << "kbluetooth_off";
        break;
    case On:
        names << "bluetooth-active" << "kbluetooth";
        break;
    case Connected:
        names << "bluetooth-connected" << "kbluetooth_connected";
        break;
    }
    return firstIcon(names, size);
}

QPixmap KBluetoothIcons::deviceIcon(const QString &icon, int size)
{
    QStringList names;
    // the theme may know the freedesktop name itself
    if (!icon.isEmpty())
        names << icon;
    // else the crystalsvg icons for it
    if (icon.startsWith("audio-") || icon == "multimedia-player")
        names << "kcmsound" << "multimedia";
    else if (icon == "input-keyboard")
        names << "keyboard";
    else if (icon == "input-mouse")
        names << "mouse";
    else if (icon == "input-gaming")
        names << "joystick";
    else if (icon == "input-tablet")
        names << "tablet";
    else if (icon == "phone")
        names << "pda_black";
    else if (icon == "computer")
        names << "system";
    else if (icon.startsWith("camera-"))
        names << "camera_unmount";
    else if (icon == "printer")
        names << "printer1";
    else if (icon == "scanner")
        names << "scanner";
    else if (icon == "video-display")
        names << "tv";
    names << "kbluetooth";
    return firstIcon(names, size);
}

QString KBluetoothIcons::deviceType(const QString &icon)
{
    if (icon == "audio-headset")
        return i18n("Headset");
    if (icon == "audio-headphones")
        return i18n("Headphones");
    if (icon == "audio-card")
        return i18n("Audio device");
    if (icon == "input-keyboard")
        return i18n("Keyboard");
    if (icon == "input-mouse")
        return i18n("Mouse");
    if (icon == "input-gaming")
        return i18n("Game controller");
    if (icon == "input-tablet")
        return i18n("Tablet");
    if (icon == "phone")
        return i18n("Phone");
    if (icon == "computer")
        return i18n("Computer");
    if (icon.startsWith("camera-"))
        return i18n("Camera");
    if (icon == "printer")
        return i18n("Printer");
    if (icon == "scanner")
        return i18n("Scanner");
    if (icon == "video-display")
        return i18n("Display");
    if (icon == "multimedia-player")
        return i18n("Media player");
    if (icon == "network-wireless")
        return i18n("Network");
    return i18n("Device");
}
