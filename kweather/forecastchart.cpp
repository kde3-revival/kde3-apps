/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include "forecastchart.h"

#include <qimage.h>
#include <qlocale.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qpointarray.h>

#include <kcalendarsystem.h>
#include <kglobal.h>
#include <klocale.h>

#include "weathericons.h"

#include <math.h>

namespace
{

QColor blend(const QColor &a, const QColor &b, double share)
{
    return QColor(int(a.red() * share + b.red() * (1 - share)),
                  int(a.green() * share + b.green() * (1 - share)),
                  int(a.blue() * share + b.blue() * (1 - share)));
}

QColor secondaryColor(const QColorGroup &cg)
{
    return blend(cg.foreground(), cg.background(), 0.6);
}

QColor rainColor(const QColorGroup &cg)
{
    return qGray(cg.background().rgb()) > 128 ? QColor(28, 104, 190) : QColor(96, 168, 240);
}

QString degrees(double v)
{
    long r = lround(v);
    return QString::number(r == 0 ? 0 : r) + QString::fromUtf8("\xc2\xb0");
}

QString field(const QStringList &fields, uint i)
{
    return i < fields.count() ? fields[i] : QString::null;
}

double number(const QStringList &fields, uint i, bool *ok)
{
    QString f = field(fields, i);
    if (f.isEmpty()) {
        *ok = false;
        return 0;
    }
    return QLocale::c().toDouble(f, ok);
}

/**
 * Shapes drawn at twice the size (coordinates and pens times 2) and
 * reduced: Qt 3 does not antialias.  The background is that of the
 * widget, so no alpha is needed.
 */
class Smooth
{
public:
    Smooth(int w, int h, const QColor &background)
        : m_w(w), m_h(h), m_pix(QMAX(1, 2 * w), QMAX(1, 2 * h))
    {
        m_pix.fill(background);
        m_p.begin(&m_pix);
    }

    QPainter &painter() { return m_p; }

    void drawOn(QPainter &p)
    {
        m_p.end();
        QImage img = m_pix.convertToImage().smoothScale(m_w, m_h);
        p.drawImage(0, 0, img);
    }

private:
    int m_w, m_h;
    QPixmap m_pix;
    QPainter m_p;
};

}

// --------------------------------------------------------------------
// DayStrip

DayStrip::DayStrip(QWidget *parent, int width, int iconSize)
    : QWidget(parent), m_current(0), m_first(0), m_width(width), m_iconSize(iconSize)
{
    setFocusPolicy(NoFocus);
    setBackgroundMode(PaletteBackground);
    setFixedWidth(width);
    setFixedHeight(sizeHint().height());
}

void DayStrip::setDays(const QValueList<ForecastDay> &days, const QDate &today)
{
    m_days = days;
    m_today = today;
    m_first = 0;
    m_current = 0;
    update();
}

int DayStrip::cardWidth() const
{
    QFontMetrics fm(font());
    int w = QMAX(fm.width(i18n("Today")), fm.width(QString::fromLatin1("Wed 88")));
    w = QMAX(w, fm.width(QString::fromLatin1("-88\xb0  -88\xb0")));
    w = QMAX(w, m_iconSize);
    return QMAX(w + 14, 60);
}

int DayStrip::arrowWidth() const
{
    return m_days.count() * cardWidth() > uint(m_width) ? 16 : 0;
}

int DayStrip::visibleCards() const
{
    int n = (m_width - 2 * arrowWidth()) / cardWidth();
    return QMAX(1, QMIN(n, int(m_days.count())));
}

QRect DayStrip::cardRect(int visible) const
{
    int n = visibleCards();
    int aw = arrowWidth();
    int w = (m_width - 2 * aw) / QMAX(1, n);
    return QRect(aw + visible * w, 0, w, height());
}

QSize DayStrip::sizeHint() const
{
    QFontMetrics fm(font());
    return QSize(m_width, 6 + fm.height() + 2 + m_iconSize + 2 + fm.height() + fm.height() + 6);
}

void DayStrip::setCurrent(int index)
{
    if (m_days.isEmpty())
        return;
    index = QMAX(0, QMIN(index, int(m_days.count()) - 1));
    m_current = index;
    int n = visibleCards();
    if (m_current < m_first)
        m_first = m_current;
    else if (m_current >= m_first + n)
        m_first = m_current - n + 1;
    update();
}

void DayStrip::mousePressEvent(QMouseEvent *e)
{
    int n = visibleCards();
    int aw = arrowWidth();
    if (aw && e->x() < aw) {
        m_first = QMAX(0, m_first - n);
        update();
        return;
    }
    if (aw && e->x() >= m_width - aw) {
        m_first = QMAX(0, QMIN(m_first + n, int(m_days.count()) - n));
        update();
        return;
    }
    for (int j = 0; j < n; ++j) {
        if (cardRect(j).contains(e->pos()) && m_first + j < int(m_days.count())) {
            m_current = m_first + j;
            update();
            emit selected(m_current);
            return;
        }
    }
}

void DayStrip::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QColorGroup &cg = palette().active();
    QFontMetrics fm(font());
    QFont small = font();
    if (small.pointSizeFloat() > 0)
        small.setPointSizeFloat(small.pointSizeFloat() * 0.85);
    else if (small.pixelSize() > 0)
        small.setPixelSize(QMAX(6, small.pixelSize() * 85 / 100));
    QFont bold = font();
    bold.setBold(true);
    KLocale *loc = KGlobal::locale();

    int n = visibleCards();
    for (int j = 0; j < n; ++j) {
        int i = m_first + j;
        if (i >= int(m_days.count()))
            break;
        const ForecastDay &d = m_days[i];
        QRect r = cardRect(j);
        r.addCoords(2, 1, -2, -1);
        if (i == m_current) {
            p.setPen(cg.highlight());
            p.setBrush(blend(cg.highlight(), cg.background(), 0.18));
            p.drawRoundRect(r, 1200 / QMAX(r.width(), 1), 1200 / QMAX(r.height(), 1));
        }
        int y = r.top() + 5;
        QString name = d.date == m_today ? i18n("Today")
                     : loc->calendar()->weekDayName(d.date, true) + QChar(' ')
                       + QString::number(d.date.day());
        p.setFont(i == m_current ? bold : font());
        p.setPen(cg.foreground());
        p.drawText(QRect(r.left(), y, r.width(), fm.height()), Qt::AlignCenter, name);
        y += fm.height() + 2;
        p.drawPixmap(r.left() + (r.width() - m_iconSize) / 2, y,
                     WeatherIcons::load(d.icon, m_iconSize));
        y += m_iconSize + 2;
        if (d.hasTemperature) {
            QString hi = degrees(d.high), lo = degrees(d.low);
            QFontMetrics bm(bold);
            int total = bm.width(hi) + fm.width(QString::fromLatin1("  ")) + fm.width(lo);
            int x = r.left() + (r.width() - total) / 2;
            p.setFont(bold);
            p.setPen(cg.foreground());
            p.drawText(x, y + fm.ascent(), hi);
            x += bm.width(hi) + fm.width(QString::fromLatin1("  "));
            p.setFont(font());
            p.setPen(secondaryColor(cg));
            p.drawText(x, y + fm.ascent(), lo);
        }
        y += fm.height();
        if (!d.precipitation.isEmpty()) {
            p.setFont(small);
            p.setPen(rainColor(cg));
            p.drawText(QRect(r.left(), y, r.width(), fm.height()), Qt::AlignCenter, d.precipitation);
        }
    }

    // the arrows: more days before or after
    int aw = arrowWidth();
    if (aw) {
        int cy = height() / 2;
        QPointArray left(3), right(3);
        left.setPoints(3, aw - 4, cy - 7, aw - 4, cy + 7, 3, cy);
        right.setPoints(3, m_width - aw + 4, cy - 7, m_width - aw + 4, cy + 7, m_width - 3, cy);
        p.setPen(Qt::NoPen);
        p.setBrush(m_first > 0 ? cg.foreground() : blend(cg.foreground(), cg.background(), 0.25));
        p.drawPolygon(left);
        p.setBrush(m_first + n < int(m_days.count()) ? cg.foreground()
                                                      : blend(cg.foreground(), cg.background(), 0.25));
        p.drawPolygon(right);
    }
}

// --------------------------------------------------------------------
// HourChart

HourChart::HourChart(QWidget *parent, int height)
    : QWidget(parent), m_kind(Temperature), m_height(height)
{
    setFocusPolicy(NoFocus);
    setBackgroundMode(PaletteBackground);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(height);
}

QSize HourChart::sizeHint() const
{
    return QSize(400, m_height);
}

void HourChart::setHours(const QStringList &hours)
{
    m_points.clear();
    m_tempUnit = m_precipUnit = m_windUnit = QString::null;
    for (QStringList::ConstIterator it = hours.begin(); it != hours.end(); ++it) {
        QStringList f = QStringList::split(QChar('\t'), *it, true);
        Point pt;
        pt.time = QDateTime::fromString(field(f, 0), Qt::ISODate).time();
        pt.temperature = number(f, 2, &pt.hasTemperature);
        pt.precipitation = number(f, 3, &pt.hasPrecipitation);
        pt.probability = number(f, 4, &pt.hasProbability);
        pt.windSpeed = number(f, 6, &pt.hasWind);
        pt.windDirection = number(f, 7, &pt.hasDirection);
        bool ok;
        pt.stamp = long(number(f, 11, &ok));
        if (!ok)
            pt.stamp = 0;
        // one scale per quantity: the units of the first hour; hours in
        // other units (from a download in the old units) are left out
        if (m_tempUnit.isEmpty())
            m_tempUnit = field(f, 8);
        if (m_precipUnit.isEmpty())
            m_precipUnit = field(f, 9);
        if (m_windUnit.isEmpty())
            m_windUnit = field(f, 10);
        if (field(f, 8) != m_tempUnit)
            pt.hasTemperature = false;
        if (field(f, 9) != m_precipUnit)
            pt.hasPrecipitation = pt.hasProbability = false;
        if (field(f, 10) != m_windUnit)
            pt.hasWind = pt.hasDirection = false;
        m_points.append(pt);
    }
    update();
}

void HourChart::setKind(int kind)
{
    m_kind = kind;
    update();
}

QString HourChart::unit() const
{
    return m_kind == Temperature ? m_tempUnit
         : m_kind == Precipitation ? m_precipUnit : m_windUnit;
}

double HourChart::x(int i) const
{
    // by time: a missing hour leaves its place empty
    int n = m_points.count();
    long first = n ? m_points[0].stamp : 0, last = n ? m_points[n - 1].stamp : 0;
    if (n && first > 0 && last >= first && m_points[i].stamp >= first)
        return (m_points[i].stamp - first + 1800.0) / (last - first + 3600.0) * width();
    return (i + 0.5) * width() / QMAX(1, n);
}

QString HourChart::valueLabel(int i) const
{
    const Point &pt = m_points[i];
    if (m_kind == Temperature)
        return pt.hasTemperature ? degrees(pt.temperature) : QString::null;
    if (m_kind == Precipitation)
        return pt.hasProbability ? i18n("%1 %").arg(long(pt.probability))
             : pt.hasPrecipitation ? KGlobal::locale()->formatNumber(pt.precipitation,
                                                                     m_precipUnit == "in" ? 2 : 1)
             : QString::null;
    return pt.hasWind ? QString::number(lround(pt.windSpeed)) : QString::null;
}

/** A label of width w centred at @p cx, moved inside the widget. */
static QRect labelRect(int cx, int top, int w, int h, int widgetWidth)
{
    int left = QMAX(0, QMIN(cx - w / 2, widgetWidth - w));
    return QRect(left, top, w, h);
}

int HourChart::labelStep() const
{
    // every 3 hours like the hours of a clock face, more apart when narrow
    // the room of the widest label, time or value (bold), as shown
    QFont bold = font();
    bold.setBold(true);
    QFontMetrics fm(font()), bm(bold);
    int n = m_points.count();
    int room = 0;
    for (int i = 0; i < n; ++i) {
        room = QMAX(room, fm.width(KGlobal::locale()->formatTime(m_points[i].time)));
        room = QMAX(room, bm.width(valueLabel(i)));
    }
    room += 10;
    static const int steps[] = { 1, 2, 3, 4, 6, 8, 12, 24 };
    for (int i = 0; i < 8; ++i) {
        int s = steps[i];
        if (n >= 12 && s < 3)
            continue;
        if ((n + s - 1) / s * room <= width())
            return s;
    }
    return 24;
}

void HourChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QColorGroup &cg = palette().active();
    QFontMetrics fm(font());
    int n = m_points.count();
    if (!n) {
        p.setPen(secondaryColor(cg));
        p.drawText(rect(), Qt::AlignCenter, i18n("No hourly forecast for this day"));
        return;
    }
    const int top = fm.height() + 6;
    const int bottom = height() - fm.height() - 6;
    const int step = labelStep();
    KLocale *loc = KGlobal::locale();

    if (m_kind == Temperature) {
        double lo = 1e9, hi = -1e9;
        for (int i = 0; i < n; ++i)
            if (m_points[i].hasTemperature) {
                lo = QMIN(lo, m_points[i].temperature);
                hi = QMAX(hi, m_points[i].temperature);
            }
        if (lo > hi) {
            p.setPen(secondaryColor(cg));
            p.drawText(rect(), Qt::AlignCenter, i18n("No temperatures for this day"));
        } else {
            double range = QMAX(hi - lo, 4.0);
            double base = lo - range * 0.35, peak = hi + range * 0.05;
            // the curve: Catmull-Rom through the hours
            QValueList<int> idx;
            for (int i = 0; i < n; ++i)
                if (m_points[i].hasTemperature)
                    idx.append(i);
            Smooth s(width(), height(), cg.background());
            QPainter &sp = s.painter();
            int m = idx.count();
            QPointArray area(m * 8 + 3);
            int k = 0;
            for (int j = 0; j < m; ++j) {
                int i0 = idx[QMAX(0, j - 1)], i1 = idx[j], i2 = idx[QMIN(m - 1, j + 1)],
                    i3 = idx[QMIN(m - 1, j + 2)];
                double y0 = m_points[i0].temperature, y1 = m_points[i1].temperature,
                       y2 = m_points[i2].temperature, y3 = m_points[i3].temperature;
                int sub = j + 1 < m ? 8 : 1;
                for (int t = 0; t < sub; ++t) {
                    double u = double(t) / 8;
                    double v = 0.5 * ((2 * y1) + (-y0 + y2) * u + (2 * y0 - 5 * y1 + 4 * y2 - y3) * u * u
                                      + (-y0 + 3 * y1 - 3 * y2 + y3) * u * u * u);
                    double xx = x(i1) + (x(i2) - x(i1)) * u;
                    double yy = bottom - (v - base) / (peak - base) * (bottom - top);
                    area.setPoint(k++, int(xx * 2), int(yy * 2));
                }
            }
            QPointArray line(k);
            for (int j = 0; j < k; ++j)
                line.setPoint(j, area.point(j));
            area.setPoint(k++, int(x(idx[m - 1]) * 2), bottom * 2);
            area.setPoint(k++, int(x(idx[0]) * 2), bottom * 2);
            area.resize(k);
            sp.setPen(Qt::NoPen);
            sp.setBrush(blend(cg.highlight(), cg.background(), 0.22));
            sp.drawPolygon(area);
            sp.setPen(QPen(cg.highlight(), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            sp.drawPolyline(line);
            s.drawOn(p);
            // the values, every few hours
            QFont bold = font();
            bold.setBold(true);
            p.setFont(bold);
            p.setPen(cg.foreground());
            for (int i = 0; i < n; i += step) {
                if (!m_points[i].hasTemperature)
                    continue;
                int yy = int(bottom - (m_points[i].temperature - base) / (peak - base) * (bottom - top));
                QString t = valueLabel(i);
                p.drawText(labelRect(int(x(i)), yy - fm.height() - 3, QFontMetrics(bold).width(t) + 4,
                                     fm.height(), width()), Qt::AlignCenter, t);
            }
            p.setFont(font());
        }
    } else if (m_kind == Precipitation) {
        bool inch = m_precipUnit == "in";
        double most = inch ? 0.04 : 1.0;            // the scale goes at least to 1 mm
        bool any = false, known = false;
        for (int i = 0; i < n; ++i) {
            if (m_points[i].hasPrecipitation || m_points[i].hasProbability)
                known = true;
            if (m_points[i].hasPrecipitation)
                most = QMAX(most, m_points[i].precipitation);
            if ((m_points[i].hasPrecipitation && m_points[i].precipitation > 0)
                || (m_points[i].hasProbability && m_points[i].probability >= 10))
                any = true;
        }
        // the width of an hour (by time, so gaps stay gaps)
        int bw = QMAX(2, int((n > 1 ? (x(n - 1) - x(0)) / (n - 1) : width()) * 0.6));
        p.setPen(Qt::NoPen);
        p.setBrush(rainColor(cg));
        for (int i = 0; i < n; ++i) {
            if (!m_points[i].hasPrecipitation || m_points[i].precipitation <= 0)
                continue;
            int h = QMAX(2, int(m_points[i].precipitation / most * (bottom - top)));
            p.drawRect(int(x(i)) - bw / 2, bottom - h, bw, h);
        }
        p.setPen(secondaryColor(cg));
        p.drawLine(0, bottom, width(), bottom);
        if (!known) {
            // unknown is not dry
            p.drawText(QRect(0, top, width(), bottom - top), Qt::AlignCenter,
                       i18n("The model gives no precipitation for these hours"));
        } else if (!any) {
            p.drawText(QRect(0, top, width(), bottom - top), Qt::AlignCenter,
                       i18n("No precipitation expected"));
        } else {
            for (int i = 0; i < n; i += step) {
                QString t = valueLabel(i);
                p.drawText(labelRect(int(x(i)), 0, fm.width(t) + 4, fm.height(), width()),
                           Qt::AlignCenter, t);
            }
        }
    } else {
        // wind: the speed, and an arrow where it blows to
        double most = 1;
        for (int i = 0; i < n; ++i)
            if (m_points[i].hasWind)
                most = QMAX(most, m_points[i].windSpeed);
        Smooth s(width(), height(), cg.background());
        QPainter &sp = s.painter();
        int mid = (top + bottom) / 2;
        int len = QMAX(8, QMIN((bottom - top) / 2 - 2, 22));
        for (int i = 0; i < n; i += step) {
            const Point &pt = m_points[i];
            if (!pt.hasWind || !pt.hasDirection || pt.windSpeed < 0.5)
                continue;
            double a = (pt.windDirection + 180) * 3.14159265 / 180;
            double l = len * (0.6 + 0.4 * pt.windSpeed / most);
            double dx = sin(a) * l, dy = -cos(a) * l;
            double cx = x(i), cy = mid;
            QColor c = blend(cg.foreground(), cg.background(), 0.8);
            sp.setPen(QPen(c, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            sp.drawLine(int((cx - dx / 2) * 2), int((cy - dy / 2) * 2),
                        int((cx + dx / 2) * 2), int((cy + dy / 2) * 2));
            // the head
            double hx = cx + dx / 2, hy = cy + dy / 2;
            double ux = dx / l, uy = dy / l;
            QPointArray head(3);
            head.setPoint(0, int((hx + ux * 3) * 2), int((hy + uy * 3) * 2));
            head.setPoint(1, int((hx - ux * 6 - uy * 5) * 2), int((hy - uy * 6 + ux * 5) * 2));
            head.setPoint(2, int((hx - ux * 6 + uy * 5) * 2), int((hy - uy * 6 - ux * 5) * 2));
            sp.setPen(Qt::NoPen);
            sp.setBrush(c);
            sp.drawPolygon(head);
        }
        s.drawOn(p);
        p.setPen(cg.foreground());
        for (int i = 0; i < n; i += step) {
            QString t = valueLabel(i);
            p.drawText(labelRect(int(x(i)), 0, fm.width(t) + 4, fm.height(), width()),
                       Qt::AlignCenter, t);
        }
    }

    // the hours
    p.setFont(font());
    p.setPen(secondaryColor(cg));
    for (int i = 0; i < n; i += step)
    {
        QString t = loc->formatTime(m_points[i].time);
        int w = fm.width(t) + 4;
        p.drawText(labelRect(int(x(i)), height() - fm.height() - 2, w, fm.height(), width()),
                   Qt::AlignCenter, t);
    }
}

#include "forecastchart.moc"
