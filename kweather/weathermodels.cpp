/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include "weathermodels.h"

#include <qlocale.h>

#include <klocale.h>

namespace
{

struct Model
{
    const char *name;
    const char *display;
};

// I18N_NOOP: translated in displayName()
const Model modelTable[] = {
    { "ecmwf_ifs",      I18N_NOOP("ECMWF IFS HRES 9 km") },
    { "hybrid",         I18N_NOOP("ICON-EU for the next hours, ECMWF IFS HRES for the days") },
    { "icon_eu",        I18N_NOOP("DWD ICON-EU 7 km (5 days)") },
    { "icon_seamless",  I18N_NOOP("DWD ICON, seamless") },
    { "ecmwf_ifs025",   I18N_NOOP("ECMWF IFS 0.25\xc2\xb0") },
    { "gfs_seamless",   I18N_NOOP("NOAA GFS, seamless") },
    { "meteofrance_seamless", I18N_NOOP("M\xc3\xa9t\xc3\xa9o-France, seamless") },
    { "best_match",     I18N_NOOP("Open-Meteo best match") },
    { 0, 0 }
};

}

QString WeatherModels::defaultModel()
{
    return QString::fromLatin1("ecmwf_ifs");
}

QStringList WeatherModels::models()
{
    QStringList list;
    for (const Model *m = modelTable; m->name; ++m)
        list.append(QString::fromLatin1(m->name));
    return list;
}

QString WeatherModels::displayName(const QString &model)
{
    for (const Model *m = modelTable; m->name; ++m)
        if (model == QString::fromLatin1(m->name))
            return i18n(m->display);
    return model;
}

bool WeatherModels::isKnown(const QString &model)
{
    for (const Model *m = modelTable; m->name; ++m)
        if (model == QString::fromLatin1(m->name))
            return true;
    return false;
}

QString WeatherModels::locationId(double latitude, double longitude,
                                  const QString &model)
{
    QLocale c = QLocale::c();
    return c.toString(latitude, 'f', 4) + QChar(',')
         + c.toString(longitude, 'f', 4) + QChar(',') + model;
}

bool WeatherModels::parseLocationId(const QString &id, double *latitude,
                                    double *longitude, QString *model)
{
    QStringList parts = QStringList::split(QChar(','), id, true);
    if (parts.count() != 3)
        return false;
    QLocale c = QLocale::c();
    bool okLat = false, okLon = false;
    double lat = c.toDouble(parts[0], &okLat);
    double lon = c.toDouble(parts[1], &okLon);
    if (!okLat || !okLon || lat < -90 || lat > 90 || lon < -180 || lon > 180)
        return false;
    if (!isKnown(parts[2]))
        return false;
    *latitude = lat;
    *longitude = lon;
    *model = parts[2];
    return true;
}
