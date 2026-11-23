/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include "weathericons.h"

#include <qimage.h>

#include <kglobal.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kstandarddirs.h>

namespace
{

struct Icon
{
    const char *name;           // the theme name iconName() returns
    const char *alternative;    // older or other themes, may be 0
    const char *fallback;       // data/kweather/<fallback>.png
};

const Icon icons[] = {
    { "weather-clear",                   0,                           "sunny" },
    { "weather-clear-night",             0,                           "sunny_night" },
    { "weather-few-clouds",              0,                           "cloudy1" },
    { "weather-few-clouds-night",        0,                           "cloudy1_night" },
    { "weather-clouds",                  "weather-few-clouds",        "cloudy2" },
    { "weather-clouds-night",            "weather-few-clouds-night",  "cloudy2_night" },
    { "weather-overcast",                "weather-many-clouds",       "overcast" },
    { "weather-fog",                     "weather-mist",              "fog" },
    { "weather-showers-scattered",       "weather-showers",           "light_rain" },
    { "weather-showers",                 0,                           "shower2" },
    { "weather-showers-day",             "weather-showers-scattered", "shower1" },
    { "weather-showers-night",           "weather-showers-scattered", "shower1_night" },
    { "weather-freezing-rain",           "weather-snow-rain",         "sleet" },
    { "weather-snow",                    0,                           "snow3" },
    { "weather-snow-scattered",          "weather-snow",              "snow1" },
    { "weather-snow-scattered-day",      "weather-snow-scattered",    "snow1" },
    { "weather-snow-scattered-night",    "weather-snow-scattered",    "snow1_night" },
    { "weather-storm",                   0,                           "tstorm2" },
    { "weather-storm-night",             "weather-storm",             "tstorm2_night" },
    { "weather-hail",                    "weather-storm",             "hail" },
    { "weather-none-available",          0,                           "dunno" },
    { 0, 0, 0 }
};

const Icon *findIcon(const QString &name)
{
    for (const Icon *i = icons; i->name; ++i)
        if (name == QString::fromLatin1(i->name))
            return i;
    return 0;
}

}

QString WeatherIcons::iconName(int code, bool day)
{
    const char *name;
    switch (code) {
    case 0:                                         // clear sky
        name = day ? "weather-clear" : "weather-clear-night";
        break;
    case 1:                                         // mainly clear
        name = day ? "weather-few-clouds" : "weather-few-clouds-night";
        break;
    case 2:                                         // partly cloudy
        name = day ? "weather-clouds" : "weather-clouds-night";
        break;
    case 3:                                         // overcast
        name = "weather-overcast";
        break;
    case 45: case 48:                               // fog
        name = "weather-fog";
        break;
    case 51: case 53: case 55:                      // drizzle
        name = "weather-showers-scattered";
        break;
    case 56: case 57: case 66: case 67:             // freezing drizzle, rain
        name = "weather-freezing-rain";
        break;
    case 61: case 63: case 65:                      // rain
        name = "weather-showers";
        break;
    case 71: case 73: case 75:                      // snow fall
        name = "weather-snow";
        break;
    case 77:                                        // snow grains
        name = "weather-snow-scattered";
        break;
    case 80: case 81: case 82:                      // rain showers
        name = day ? "weather-showers-day" : "weather-showers-night";
        break;
    case 85: case 86:                               // snow showers
        name = day ? "weather-snow-scattered-day" : "weather-snow-scattered-night";
        break;
    case 95:                                        // thunderstorm
        name = day ? "weather-storm" : "weather-storm-night";
        break;
    case 96: case 99:                               // thunderstorm with hail
        name = "weather-hail";
        break;
    default:
        name = "weather-none-available";
    }
    return QString::fromLatin1(name);
}

QString WeatherIcons::description(int code)
{
    switch (code) {
    case 0:  return i18n("Clear sky");
    case 1:  return i18n("Mainly clear");
    case 2:  return i18n("Partly cloudy");
    case 3:  return i18n("Overcast");
    case 45: return i18n("Fog");
    case 48: return i18n("Freezing fog");
    case 51: return i18n("Light drizzle");
    case 53: return i18n("Drizzle");
    case 55: return i18n("Dense drizzle");
    case 56: return i18n("Light freezing drizzle");
    case 57: return i18n("Freezing drizzle");
    case 61: return i18n("Light rain");
    case 63: return i18n("Rain");
    case 65: return i18n("Heavy rain");
    case 66: return i18n("Light freezing rain");
    case 67: return i18n("Freezing rain");
    case 71: return i18n("Light snow");
    case 73: return i18n("Snow");
    case 75: return i18n("Heavy snow");
    case 77: return i18n("Snow grains");
    case 80: return i18n("Light rain showers");
    case 81: return i18n("Rain showers");
    case 82: return i18n("Violent rain showers");
    case 85: return i18n("Light snow showers");
    case 86: return i18n("Snow showers");
    case 95: return i18n("Thunderstorm");
    case 96: return i18n("Thunderstorm with light hail");
    case 99: return i18n("Thunderstorm with hail");
    default: return i18n("No data");
    }
}

QString WeatherIcons::fallbackFile(const QString &name)
{
    const Icon *icon = findIcon(name);
    QString file = QString::fromLatin1(icon ? icon->fallback : "dunno");
    return locate("data", QString::fromLatin1("kweather/") + file
                  + QString::fromLatin1(".png"));
}

QPixmap WeatherIcons::load(const QString &name, int size)
{
    KIconLoader *loader = KGlobal::iconLoader();
    QPixmap pix = loader->loadIcon(name, KIcon::Panel, size,
                                   KIcon::DefaultState, 0, true);
    const Icon *icon = findIcon(name);
    if (pix.isNull() && icon && icon->alternative)
        pix = loader->loadIcon(QString::fromLatin1(icon->alternative),
                               KIcon::Panel, size, KIcon::DefaultState, 0, true);
    if (pix.isNull()) {
        QImage image(fallbackFile(name));
        if (!image.isNull() && size > 0)
            image = image.smoothScale(size, size, QImage::ScaleMin);
        pix.convertFromImage(image);
    }
    return pix;
}
