/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef KWEATHER_WEATHERMODELS_H
#define KWEATHER_WEATHERMODELS_H

#include <qstring.h>
#include <qstringlist.h>

/**
 * The forecast models offered in the settings: Open-Meteo's "models="
 * names, plus "hybrid" (ICON-EU for the current weather and the next
 * hours, the ECMWF IFS HRES for the days).
 */
namespace WeatherModels
{
    /** The default: the ECMWF IFS HRES 9 km. */
    QString defaultModel();

    /** The model names, in the order of the settings. */
    QStringList models();

    /** The name shown for @p model, translated. */
    QString displayName(const QString &model);

    /** Whether @p model is in models(). */
    bool isKnown(const QString &model);

    /** The location key of the service: "<latitude>,<longitude>,<model>". */
    QString locationId(double latitude, double longitude, const QString &model);

    /** Splits a location key; false if it is not one. */
    bool parseLocationId(const QString &id, double *latitude, double *longitude,
                         QString *model);
}

#endif
