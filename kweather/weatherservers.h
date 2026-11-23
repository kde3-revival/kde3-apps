/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef KWEATHER_WEATHERSERVERS_H
#define KWEATHER_WEATHERSERVERS_H

#include <qstring.h>

/**
 * The APIs of Open-Meteo.  [Open-Meteo] forecast_url and geocoding_url
 * in kweatherservicerc replace them, e.g. for a self-hosted Open-Meteo
 * (no settings page: it is for administrators).
 */
namespace WeatherServers
{
    inline QString forecast()
    {
        return QString::fromLatin1("https://api.open-meteo.com/v1/forecast");
    }

    inline QString geocoding()
    {
        return QString::fromLatin1("https://geocoding-api.open-meteo.com/v1/search");
    }
}

#endif
