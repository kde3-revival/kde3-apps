/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef KWEATHER_WEATHERICONS_H
#define KWEATHER_WEATHERICONS_H

#include <qpixmap.h>
#include <qstring.h>

/**
 * The weather codes of Open-Meteo (WMO 4677 "ww", as reduced by
 * Open-Meteo) as icon names of the freedesktop/KDE icon themes and as
 * text.  The pictures of KWeather 3.5 (PNG files in data/kweather) stand in for
 * the theme icons the theme lacks.
 */
namespace WeatherIcons
{
    /** The theme icon name for @p code (weather-clear, ...), -1 = no data. */
    QString iconName(int code, bool day);

    /** The description of @p code ("Light rain", ...), translated. */
    QString description(int code);

    /**
     * The icon @p name (from iconName()) at @p size pixels: from the icon
     * theme, else an alternative name, else the KWeather 3.5 picture.
     */
    QPixmap load(const QString &name, int size);

    /** The path of the KWeather 3.5 picture for @p name (may be empty). */
    QString fallbackFile(const QString &name);
}

#endif
