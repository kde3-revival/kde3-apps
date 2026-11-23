/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef KWEATHER_FORECASTCHART_H
#define KWEATHER_FORECASTCHART_H

#include <qdatetime.h>
#include <qstringlist.h>
#include <qvaluelist.h>
#include <qwidget.h>

/** A day of the forecast, as the popup shows it. */
struct ForecastDay
{
    QDate date;
    QString icon;
    QString description;
    double high, low;           // valid when hasTemperature
    bool hasTemperature;
    QString precipitation;      // shown amount, empty if none
};

/**
 * The days of the forecast as a row of cards (weekday, icon, maximum and
 * minimum, rain), one of them selected; arrows at the ends when there are
 * more days than cards.  Clicks select; the popup passes the keys.
 */
class DayStrip : public QWidget
{
    Q_OBJECT

public:
    DayStrip(QWidget *parent, int width, int iconSize);

    void setDays(const QValueList<ForecastDay> &days, const QDate &today);
    int current() const { return m_current; }
    int count() const { return m_days.count(); }
    void setCurrent(int index);

    QSize sizeHint() const;
    QSize minimumSizeHint() const { return sizeHint(); }

signals:
    void selected(int index);

protected:
    void paintEvent(QPaintEvent *e);
    void mousePressEvent(QMouseEvent *e);

private:
    int arrowWidth() const;
    int cardWidth() const;
    int visibleCards() const;
    QRect cardRect(int visible) const;

    QValueList<ForecastDay> m_days;
    QDate m_today;
    int m_current;
    int m_first;
    int m_width;
    int m_iconSize;
};

/**
 * The hours of a day as a chart: temperature (a curve with the area
 * under it), precipitation (bars, with the probability) or wind (speed
 * and an arrow for the direction).
 */
class HourChart : public QWidget
{
    Q_OBJECT

public:
    enum Kind { Temperature = 0, Precipitation = 1, Wind = 2 };

    HourChart(QWidget *parent, int height);

    /** Entries of WeatherService::dayHours(). */
    void setHours(const QStringList &hours);
    void setKind(int kind);
    int kind() const { return m_kind; }
    /** The units of the shown kind, e.g. "°C". */
    QString unit() const;

    QSize sizeHint() const;

protected:
    void paintEvent(QPaintEvent *e);

private:
    struct Point
    {
        QTime time;
        double temperature, precipitation, probability, windSpeed, windDirection;
        bool hasTemperature, hasPrecipitation, hasProbability, hasWind, hasDirection;
        long stamp;     // unix time, 0 if unknown
    };

    double x(int i) const;
    QString valueLabel(int i) const;
    int labelStep() const;

    QValueList<Point> m_points;
    QString m_tempUnit, m_precipUnit, m_windUnit;
    int m_kind;
    int m_height;
};

#endif
