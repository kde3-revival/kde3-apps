/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef KWEATHER_JSON_H
#define KWEATHER_JSON_H

#include <qcstring.h>
#include <qvariant.h>

/**
 * A small JSON reader for the answers of Open-Meteo: objects become
 * QVariant maps, arrays QVariant lists, numbers doubles, strings
 * QStrings, true and false bools and null an invalid QVariant.
 */
namespace Json
{
    /** Parses @p text (UTF-8); @p ok is false on a syntax error. */
    QVariant parse(const QByteArray &text, bool *ok = 0);

    /** The member @p key of the object @p v (invalid if none). */
    QVariant member(const QVariant &v, const char *key);

    /** @p v as a double, or @p def when it is not a number. */
    double number(const QVariant &v, double def);
}

#endif
