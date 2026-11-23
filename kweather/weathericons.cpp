/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include "weathericons.h"

#include <qbitmap.h>
#include <qimage.h>
#include <qmap.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qpointarray.h>

#include <kglobal.h>
#include <kglobalsettings.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kstandarddirs.h>

#include <math.h>

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

namespace
{

// The icons are drawn at four times their size, with a mask, and reduced
// with a box filter on premultiplied alpha: Qt 3's QPainter does not
// antialias.  Coordinates are in a 100 x 100 square.
class Canvas
{
public:
    explicit Canvas(int size)
        : m_size(size), m_scale(size * 4 / 100.0),
          m_pix(size * 4, size * 4), m_mask(size * 4, size * 4)
    {
        m_pix.fill(Qt::black);
        m_mask.fill(Qt::color0);
        m_p.begin(&m_pix);
        m_m.begin(&m_mask);
    }

    int s(double v) const { return static_cast<int>(v * m_scale + 0.5); }

    void disc(double cx, double cy, double r, const QColor &c)
    {
        QRect rect(s(cx - r), s(cy - r), s(2 * r), s(2 * r));
        m_p.setPen(Qt::NoPen);
        m_p.setBrush(c);
        m_p.drawEllipse(rect);
        m_m.setPen(Qt::NoPen);
        m_m.setBrush(Qt::color1);
        m_m.drawEllipse(rect);
    }

    void cut(double cx, double cy, double r)
    {
        QRect rect(s(cx - r), s(cy - r), s(2 * r), s(2 * r));
        m_m.setPen(Qt::NoPen);
        m_m.setBrush(Qt::color0);
        m_m.drawEllipse(rect);
    }

    void box(double x, double y, double w, double h, double radius, const QColor &c)
    {
        QRect rect(s(x), s(y), s(w), s(h));
        int rx = w > 0 ? static_cast<int>(200 * radius / w) : 0;
        int ry = h > 0 ? static_cast<int>(200 * radius / h) : 0;
        m_p.setPen(Qt::NoPen);
        m_p.setBrush(c);
        m_p.drawRoundRect(rect, QMIN(rx, 99), QMIN(ry, 99));
        m_m.setPen(Qt::NoPen);
        m_m.setBrush(Qt::color1);
        m_m.drawRoundRect(rect, QMIN(rx, 99), QMIN(ry, 99));
    }

    void line(double x1, double y1, double x2, double y2, double width, const QColor &c)
    {
        m_p.setPen(QPen(c, uint(QMAX(1, s(width))), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        m_p.drawLine(s(x1), s(y1), s(x2), s(y2));
        m_m.setPen(QPen(Qt::color1, uint(QMAX(1, s(width))), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        m_m.drawLine(s(x1), s(y1), s(x2), s(y2));
    }

    void polygon(const double *xy, int points, const QColor &c)
    {
        QPointArray a(points);
        for (int i = 0; i < points; ++i)
            a.setPoint(i, s(xy[2 * i]), s(xy[2 * i + 1]));
        m_p.setPen(Qt::NoPen);
        m_p.setBrush(c);
        m_p.drawPolygon(a);
        m_m.setPen(Qt::NoPen);
        m_m.setBrush(Qt::color1);
        m_m.drawPolygon(a);
    }

    void text(const QString &t, const QColor &c)
    {
        QFont f = KGlobalSettings::generalFont();
        f.setBold(true);
        f.setPixelSize(s(46));
        m_p.setFont(f);
        m_p.setPen(c);
        m_p.drawText(QRect(0, s(20), s(100), s(70)), Qt::AlignCenter, t);
        m_m.setFont(f);
        m_m.setPen(Qt::color1);
        m_m.drawText(QRect(0, s(20), s(100), s(70)), Qt::AlignCenter, t);
    }

    QPixmap result()
    {
        m_p.end();
        m_m.end();
        QImage big = m_pix.convertToImage().convertDepth(32);
        QImage mask = m_mask.convertToImage();
        QImage out(m_size, m_size, 32);
        out.setAlphaBuffer(true);
        for (int y = 0; y < m_size; ++y) {
            for (int x = 0; x < m_size; ++x) {
                int r = 0, g = 0, b = 0, a = 0;
                for (int j = 0; j < 4; ++j) {
                    for (int i = 0; i < 4; ++i) {
                        int bx = x * 4 + i, by = y * 4 + j;
                        if (!mask.pixelIndex(bx, by))
                            continue;
                        QRgb c = big.pixel(bx, by);
                        r += qRed(c);
                        g += qGreen(c);
                        b += qBlue(c);
                        ++a;
                    }
                }
                out.setPixel(x, y, a ? qRgba(r / a, g / a, b / a, a * 255 / 16)
                                     : qRgba(0, 0, 0, 0));
            }
        }
        QPixmap pix;
        pix.convertFromImage(out);
        return pix;
    }

private:
    int m_size;
    double m_scale;
    QPixmap m_pix;
    QBitmap m_mask;
    QPainter m_p;
    QPainter m_m;
};

// a palette that reads on light and on dark backgrounds: every shape has a
// darker rim
const QColor sunFill(255, 196, 30), sunRim(214, 128, 0);
const QColor moonFill(246, 236, 190), moonRim(168, 150, 90);
const QColor cloudFill(246, 248, 250), cloudRim(124, 136, 150);
const QColor greyFill(196, 204, 213), greyRim(98, 110, 124);
const QColor darkFill(132, 142, 156), darkRim(70, 78, 90);
const QColor rain(48, 132, 220), snowColor(110, 170, 235), boltFill(255, 204, 0),
             boltRim(176, 120, 0), fog(150, 160, 172);

enum CloudKind { Light, Grey, Dark };

void sun(Canvas &c, double cx, double cy, double r)
{
    for (int i = 0; i < 8; ++i) {
        double a = i * 3.14159265 / 4;
        c.line(cx + cos(a) * r * 1.35, cy + sin(a) * r * 1.35,
               cx + cos(a) * r * 1.8, cy + sin(a) * r * 1.8, r * 0.28, sunRim);
    }
    c.disc(cx, cy, r, sunRim);
    c.disc(cx, cy, r * 0.84, sunFill);
}

void moon(Canvas &c, double cx, double cy, double r)
{
    c.disc(cx, cy, r, moonRim);
    c.disc(cx, cy, r * 0.86, moonFill);
    c.cut(cx + r * 0.55, cy - r * 0.4, r * 0.8);
}

/** A cloud of width w, its flat bottom at y = bottom. */
void cloud(Canvas &c, double cx, double bottom, double w, CloudKind kind)
{
    const QColor &fill = kind == Light ? cloudFill : kind == Grey ? greyFill : darkFill;
    const QColor &rim = kind == Light ? cloudRim : kind == Grey ? greyRim : darkRim;
    for (int pass = 0; pass < 2; ++pass) {
        double o = pass == 0 ? w * 0.045 : 0;       // the rim, then the inside
        const QColor &col = pass == 0 ? rim : fill;
        c.disc(cx - w * 0.24, bottom - w * 0.17, w * 0.17 + o, col);
        c.disc(cx + w * 0.02, bottom - w * 0.27, w * 0.25 + o, col);
        c.disc(cx + w * 0.27, bottom - w * 0.16, w * 0.16 + o, col);
        c.box(cx - w * 0.42 - o, bottom - w * 0.2 - o, w * 0.84 + 2 * o, w * 0.2 + 2 * o,
              w * 0.1 + o, col);
    }
}

void drops(Canvas &c, int n, double top, bool small)
{
    double x0 = n == 2 ? 38 : 30;
    for (int i = 0; i < n; ++i) {
        double x = x0 + i * 20;
        if (small) {
            c.disc(x, top + 8, 4.2, rain);
            c.disc(x - 7, top + 22, 4.2, rain);
        } else {
            c.line(x, top + 2, x - 8, top + 24, 7.5, rain);
        }
    }
}

void flake(Canvas &c, double cx, double cy, double r)
{
    for (int i = 0; i < 3; ++i) {
        double a = i * 3.14159265 / 3 + 3.14159265 / 2;
        c.line(cx - cos(a) * r, cy - sin(a) * r, cx + cos(a) * r, cy + sin(a) * r, r * 0.42, snowColor);
    }
}

void bolt(Canvas &c)
{
    static const double rim[] = { 52, 56, 37, 82, 49, 82, 42, 100, 66, 72, 54, 72, 62, 56 };
    static const double fill[] = { 53, 60, 42, 79, 52, 79, 47, 93, 61, 75, 50, 75, 57, 60 };
    c.polygon(rim, 7, boltRim);
    c.polygon(fill, 7, boltFill);
}

QPixmap drawIcon(const QString &name, int size)
{
    Canvas c(size);
    bool night = name.endsWith("-night");
    QString base = night ? name.left(name.length() - 6) : name;
    if (base.endsWith("-day"))
        base = base.left(base.length() - 4);

    if (base == "weather-clear") {
        if (night)
            moon(c, 50, 50, 34);
        else
            sun(c, 50, 50, 24);
    } else if (base == "weather-few-clouds" || base == "weather-clouds") {
        bool few = base == "weather-few-clouds";
        if (night)
            moon(c, 36, 34, few ? 24 : 20);
        else
            sun(c, 37, 37, few ? 17 : 15);
        cloud(c, few ? 60 : 56, 84, few ? 64 : 76, Light);
    } else if (base == "weather-overcast") {
        cloud(c, 40, 62, 60, Grey);
        cloud(c, 58, 84, 76, Light);
    } else if (base == "weather-fog") {
        cloud(c, 50, 56, 74, Grey);
        c.line(18, 70, 82, 70, 6, fog);
        c.line(26, 82, 90, 82, 6, fog);
        c.line(12, 94, 70, 94, 6, fog);
    } else if (base == "weather-showers-scattered") {
        cloud(c, 50, 64, 80, Grey);
        drops(c, 3, 68, true);
    } else if (base == "weather-showers") {
        if (name != base) {         // showers by day or night: the sun or moon behind
            if (night)
                moon(c, 32, 30, 20);
            else
                sun(c, 32, 32, 14);
            cloud(c, 58, 64, 70, Grey);
            drops(c, 2, 68, false);
        } else {
            cloud(c, 50, 64, 80, Grey);
            drops(c, 3, 68, false);
        }
    } else if (base == "weather-freezing-rain") {
        cloud(c, 50, 64, 80, Grey);
        c.line(36, 70, 28, 92, 7.5, rain);
        flake(c, 64, 82, 11);
    } else if (base == "weather-snow" || base == "weather-snow-scattered") {
        bool scattered = base == "weather-snow-scattered";
        if (scattered && name != base) {
            if (night)
                moon(c, 32, 30, 20);
            else
                sun(c, 32, 32, 14);
            cloud(c, 58, 64, 70, Light);
        } else {
            cloud(c, 50, 64, 80, Light);
        }
        if (scattered) {
            flake(c, 40, 80, 10);
            flake(c, 66, 86, 10);
        } else {
            flake(c, 28, 78, 10);
            flake(c, 52, 88, 10);
            flake(c, 74, 78, 10);
        }
    } else if (base == "weather-storm") {
        if (night)
            moon(c, 30, 28, 18);
        cloud(c, 50, 62, 82, Dark);
        bolt(c);
    } else if (base == "weather-hail") {
        cloud(c, 50, 62, 82, Dark);
        bolt(c);
        c.disc(28, 80, 6, greyRim);
        c.disc(28, 80, 4.5, Qt::white);
        c.disc(76, 86, 6, greyRim);
        c.disc(76, 86, 4.5, Qt::white);
    } else {                        // no data
        cloud(c, 50, 80, 84, Grey);
        c.text(QString::fromLatin1("?"), darkRim);
    }
    return c.result();
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
    if (size <= 0)
        size = 32;
    // the icon theme first: what the user chose
    KIconLoader *loader = KGlobal::iconLoader();
    QPixmap pix = loader->loadIcon(name, KIcon::Panel, size,
                                   KIcon::DefaultState, 0, true);
    const Icon *icon = findIcon(name);
    if (pix.isNull() && icon && icon->alternative)
        pix = loader->loadIcon(QString::fromLatin1(icon->alternative),
                               KIcon::Panel, size, KIcon::DefaultState, 0, true);
    if (!pix.isNull() && pix.width() == size)
        return pix;

    // else our own, drawn at the size asked for
    static QMap<QString, QPixmap> cache;
    QString key = name + QChar('/') + QString::number(size);
    QMap<QString, QPixmap>::ConstIterator it = cache.find(key);
    if (it != cache.end())
        return *it;
    pix = drawIcon(icon ? name : QString::fromLatin1("weather-none-available"), size);
    cache.insert(key, pix);
    return pix;
}
