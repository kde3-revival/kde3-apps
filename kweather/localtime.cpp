/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include "localtime.h"

#include <qcstring.h>
#include <qfile.h>

#include <stdlib.h>
#include <time.h>

bool LocalTime::isZone(const QString &zone)
{
    if (zone.isEmpty() || zone.length() > 64 || zone[0] == '/' || zone.contains(".."))
        return false;
    for (uint i = 0; i < zone.length(); ++i) {
        QChar c = zone[i];
        if (!(c.isLetterOrNumber() && c.unicode() < 128) && c != '/' && c != '_'
            && c != '-' && c != '+')
            return false;
    }
    const char *dir = getenv("TZDIR");
    QString base = dir && *dir ? QString::fromLocal8Bit(dir)
                               : QString::fromLatin1("/usr/share/zoneinfo");
    return QFile::exists(base + '/' + zone);
}

QDateTime LocalTime::at(long t, const QString &zone, long fixedOffset)
{
    QDateTime dt;
    if (!isZone(zone)) {
        dt.setTime_t(static_cast<uint>(t + fixedOffset), Qt::UTC);
        return dt;
    }
    // The service is single-threaded: switch TZ around localtime_r().
    const char *old = getenv("TZ");
    QCString saved = old ? QCString(old) : QCString();
    setenv("TZ", zone.latin1(), 1);
    tzset();
    time_t tt = static_cast<time_t>(t);
    struct tm tm;
    bool ok = localtime_r(&tt, &tm) != 0;
    if (old)
        setenv("TZ", saved.data(), 1);
    else
        unsetenv("TZ");
    tzset();
    if (!ok) {
        dt.setTime_t(static_cast<uint>(t + fixedOffset), Qt::UTC);
        return dt;
    }
    return QDateTime(QDate(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday),
                     QTime(tm.tm_hour, tm.tm_min, tm.tm_sec));
}
