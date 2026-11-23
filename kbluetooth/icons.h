/*
    This file is part of kbluetooth, the Bluetooth applet of the KDE3 revival

    The icons: the theme's icon when it has one, else the ones of kbluetooth.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef KBLUETOOTH_ICONS_H
#define KBLUETOOTH_ICONS_H

#include <qpixmap.h>
#include <qstring.h>

namespace KBluetoothIcons
{
    enum State { Off, On, Connected };

    // the icon of the tray, in the size of the panel
    QPixmap trayIcon(State state, int size);
    // the icon for a device, from the freedesktop name BlueZ gives (Icon)
    QPixmap deviceIcon(const QString &bluezIcon, int size);
    // a readable name of the kind of device
    QString deviceType(const QString &bluezIcon);
}

#endif
