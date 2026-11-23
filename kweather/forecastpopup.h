/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef KWEATHER_FORECASTPOPUP_H
#define KWEATHER_FORECASTPOPUP_H

#include <qdatetime.h>
#include <qframe.h>
#include <qvaluelist.h>

class DayStrip;
class HourChart;
class QLabel;
class QScrollView;
class QTabBar;

class WeatherService_stub;

/**
 * The popup of the applet: the current weather, the next hours and the
 * next 7 days, read from KWeatherService when it is built.
 */
class ForecastPopup : public QFrame
{
    Q_OBJECT

public:
    /** @p horizontalPanel: the popup goes above or below @p parent, and
        must fit into the room there. */
    ForecastPopup(WeatherService_stub *service, const QString &location,
                  bool horizontalPanel, QWidget *parent = 0, const char *name = 0);

    /** Shows the popup (hidden() is emitted when it goes; the owner
        deletes it then) next to @p anchor (global), on the side @p direction
        (KPanelApplet::Direction) and inside the screen of @p anchor. */
    void popup(const QRect &anchor, int direction);

signals:
    void hidden();

protected:
    void hideEvent(QHideEvent *e);
    /** Escape closes; left and right choose the day, up and down the chart. */
    void keyPressEvent(QKeyEvent *e);

private slots:
    void slotDay(int index);
    void slotKind(int kind);

private:
    enum { maxLevel = 5 };
    void build(WeatherService_stub *service, const QString &location, int level);
    void showDay();
    void updateCaption();

    QScrollView *m_scroll;
    QWidget *m_content;
    WeatherService_stub *m_service;
    QString m_location;
    HourChart *m_chart;
    DayStrip *m_strip;
    QTabBar *m_tabs;
    QLabel *m_dayCaption;
    QValueList<QDate> m_dates;
    QDate m_today;
    int m_day;
    int m_kind;
};

#endif
