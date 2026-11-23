/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef KWEATHER_LOCALTIME_H
#define KWEATHER_LOCALTIME_H

#include <qdatetime.h>
#include <qstring.h>

/**
 * The clock time at the location.  Open-Meteo gives one UTC offset, that
 * of the time of the request, so a forecast across a change to or from
 * summer time would be an hour off: the rules of the time zone it names
 * (from the system's zoneinfo) are used instead, and the fixed offset only
 * when the zone is unknown.
 */
namespace LocalTime
{
    /** Whether @p zone is an IANA zone name with a zoneinfo file. */
    bool isZone(const QString &zone);

    /** The clock time in @p zone (or at @p fixedOffset) of the instant @p t. */
    QDateTime at(long t, const QString &zone, long fixedOffset);
}

#endif
