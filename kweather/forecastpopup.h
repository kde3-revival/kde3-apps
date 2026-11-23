/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef KWEATHER_FORECASTPOPUP_H
#define KWEATHER_FORECASTPOPUP_H

#include <qframe.h>

class WeatherService_stub;

/**
 * The popup of the applet: the current weather, the next hours and the
 * next 7 days, read from KWeatherService when it is built.
 */
class ForecastPopup : public QFrame
{
    Q_OBJECT

public:
    ForecastPopup(WeatherService_stub *service, const QString &location,
                  QWidget *parent = 0, const char *name = 0);

    /** Shows the popup (hidden() is emitted when it goes; the owner
        deletes it then) next to @p anchor (global), on the side @p direction
        (KPanelApplet::Direction) and inside the screen of @p anchor. */
    void popup(const QRect &anchor, int direction);

signals:
    void hidden();

protected:
    void hideEvent(QHideEvent *e);
    void keyPressEvent(QKeyEvent *e);
};

#endif
