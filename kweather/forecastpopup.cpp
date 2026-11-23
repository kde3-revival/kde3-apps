/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include "forecastpopup.h"

#include <qapplication.h>
#include <qdatetime.h>
#include <qdesktopwidget.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlocale.h>
#include <qpainter.h>
#include <qstringlist.h>
#include <qstylesheet.h>
#include <qtooltip.h>

#include <kcalendarsystem.h>
#include <kdialog.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <klocale.h>
#include <kpanelapplet.h>
#include <kseparator.h>

#include "weathericons.h"
#include "weatherservice_stub.h"

#include <math.h>

namespace
{

const int hourIcon = 32;
const int dayIcon = 32;
const int nowIcon = 64;

QColor blend(const QColor &a, const QColor &b, double share)
{
    return QColor(int(a.red() * share + b.red() * (1 - share)),
                  int(a.green() * share + b.green() * (1 - share)),
                  int(a.blue() * share + b.blue() * (1 - share)));
}

/** Amounts of rain: the blue of the drops of the icons, darker or lighter
    to stand out from the background. */
QColor rainColor(const QWidget *w)
{
    QColor bg = w->palette().active().background();
    return qGray(bg.rgb()) > 128 ? QColor(28, 104, 190) : QColor(96, 168, 240);
}

/** Text of less importance: between the text and the background. */
QColor secondary(const QWidget *w)
{
    const QColorGroup &cg = w->palette().active();
    return blend(cg.foreground(), cg.background(), 0.6);
}

QString field(const QStringList &fields, uint i)
{
    return i < fields.count() ? fields[i] : QString::null;
}

double value(const QStringList &fields, uint i, bool *ok)
{
    QString f = field(fields, i);
    if (f.isEmpty()) {
        *ok = false;
        return 0;
    }
    return QLocale::c().toDouble(f, ok);
}

QString degrees(double v)
{
    long r = lround(v);
    return QString::number(r == 0 ? 0 : r) + QString::fromUtf8("\xc2\xb0");
}

/** An amount worth showing (0.05 mm on), without its unit; empty if none. */
QString amount(double v, const QString &unit)
{
    KLocale *l = KGlobal::locale();
    bool inch = unit == "in";
    if ((inch ? v * 25.4 : v) < 0.05)
        return QString::null;
    if (inch)
        return v < 0.01 ? i18n("less than an amount", "< %1").arg(l->formatNumber(0.01, 2))
                        : l->formatNumber(v, 2);
    return v < 0.1 ? i18n("less than an amount", "< %1").arg(l->formatNumber(0.1, 1))
                   : l->formatNumber(v, 1);
}

QLabel *label(QWidget *parent, const QString &text, int align, double scale = 1.0,
              bool bold = false, bool second = false)
{
    QLabel *l = new QLabel(text, parent);
    l->setAlignment(align | Qt::SingleLine);
    if (scale != 1.0 || bold) {
        QFont f = l->font();
        if (scale != 1.0) {
            // fonts given in pixels have no point size (-1)
            if (f.pointSizeFloat() > 0)
                f.setPointSizeFloat(f.pointSizeFloat() * scale);
            else if (f.pixelSize() > 0)
                f.setPixelSize(QMAX(6, int(f.pixelSize() * scale + 0.5)));
        }
        f.setBold(bold);
        l->setFont(f);
    }
    if (second)
        l->setPaletteForegroundColor(secondary(parent));
    return l;
}

QLabel *iconLabel(QWidget *parent, const QString &name, int size, const QString &tip)
{
    QLabel *l = new QLabel(parent);
    l->setPixmap(WeatherIcons::load(name, size));
    l->setAlignment(Qt::AlignCenter);
    l->setFixedSize(size, size);
    if (!tip.isEmpty())
        QToolTip::add(l, tip);
    return l;
}

/** The temperatures of a day within those of the week. */
class RangeBar : public QWidget
{
public:
    RangeBar(QWidget *parent, double weekMin, double weekMax, double lo, double hi)
        : QWidget(parent), m_min(weekMin), m_max(weekMax), m_lo(lo), m_hi(hi)
    {
        setBackgroundMode(PaletteBackground);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumWidth(60);
        setFixedHeight(fontMetrics().height());
    }

    QSize sizeHint() const { return QSize(120, fontMetrics().height()); }

protected:
    void paintEvent(QPaintEvent *)
    {
        QPainter p(this);
        const QColorGroup &cg = palette().active();
        int h = 6;
        int y = (height() - h) / 2;
        int w = width();
        p.setPen(Qt::NoPen);
        p.setBrush(blend(cg.foreground(), cg.background(), 0.15));
        p.drawRoundRect(0, y, w, h, 99 * h / QMAX(w, 1), 99);
        double span = m_max - m_min;
        int x1 = 0, x2 = w;
        if (span > 0) {
            x1 = int((m_lo - m_min) / span * (w - h));
            x2 = int((m_hi - m_min) / span * (w - h)) + h;
        }
        p.setBrush(cg.highlight());
        p.drawRoundRect(x1, y, QMAX(x2 - x1, h), h, 99 * h / QMAX(x2 - x1, h), 99);
    }

private:
    double m_min, m_max, m_lo, m_hi;
};

QWidget *separator(QWidget *parent)
{
    return new KSeparator(KSeparator::HLine, parent);
}

}

ForecastPopup::ForecastPopup(WeatherService_stub *service,
                             const QString &location, QWidget *parent,
                             const char *name)
    : QFrame(parent, name, WType_Popup)
{
    setFrameStyle(QFrame::PopupPanel | QFrame::Raised);
    setLineWidth(1);
    setMargin(0);

    const int margin = KDialog::marginHint();
    const int spacing = KDialog::spacingHint();
    const int center = Qt::AlignHCenter | Qt::AlignVCenter;
    const int right = Qt::AlignRight | Qt::AlignVCenter;
    const int left = Qt::AlignAuto | Qt::AlignVCenter;
    KLocale *loc = KGlobal::locale();

    QVBoxLayout *top = new QVBoxLayout(this, margin, spacing);

    // the width: that of 12 equal columns of the hours
    QFontMetrics fm(font());
    int column = QMAX(QMAX(fm.width(QString::fromLatin1("00:00")), fm.width(QString::fromLatin1("-88")) + fm.width(QChar(0xb0))),
                      QMAX(hourIcon, fm.width(i18n("less than an amount", "< %1").arg(loc->formatNumber(0.1, 1)))));
    column += 2 * spacing;
    // at least 36 average characters: room for the details row
    column = QMAX(column, (36 * fm.width(QChar('x')) + 11) / 12);
    // 12 hours, or fewer on a narrow screen or with a large font
    QRect screen = QApplication::desktop()->screenGeometry(parent);
    int hourColumns = (screen.width() - 2 * margin - 2 * frameWidth() - 16) / column;
    hourColumns = QMAX(4, QMIN(12, hourColumns));
    setFixedWidth(QMIN(hourColumns * column + 2 * margin + 2 * frameWidth(), screen.width()));
    // the day bars: 3 columns at full width, less when the popup is narrower
    const int barWidth = column * QMAX(1, QMIN(3, hourColumns - 9));

    // the place, and when the data is from
    QHBoxLayout *header = new QHBoxLayout(top, spacing);
    QString place = service->stationName(location);
    QString country = service->stationCountry(location);
    header->addWidget(label(this, country.isEmpty() ? place
                            : i18n("place, country", "%1, %2").arg(place).arg(country),
                            left, 1.2, true));
    header->addStretch();
    QDateTime updated = QDateTime::fromString(service->currentTime(location), Qt::ISODate);
    if (updated.isValid())
        header->addWidget(label(this, i18n("Updated %1").arg(loc->formatTime(updated.time())),
                                right | Qt::AlignBottom, 0.85, false, true));
    top->addWidget(separator(this));

    QString error = service->error(location);
    QString temperature = service->temperature(location);
    if (temperature.isEmpty()) {
        QLabel *l = new QLabel(error.isEmpty() ? i18n("Waiting for the weather data...")
                               : i18n("The weather could not be fetched: %1").arg(error), this);
        l->setAlignment(Qt::AlignCenter | Qt::WordBreak);
        l->setMinimumHeight(4 * fm.height());
        top->addWidget(l);
    } else {
        // now: the icon, the temperature, the sky and how it feels
        QHBoxLayout *now = new QHBoxLayout(top, 2 * spacing);
        now->addWidget(iconLabel(this, service->currentIconString(location), nowIcon, QString::null));
        now->addWidget(label(this, temperature, left, 2.4, true));
        QVBoxLayout *sky = new QVBoxLayout(now, 0);
        sky->addStretch();
        sky->addWidget(label(this, service->weather(location).join(QString::fromLatin1(", ")),
                             left, 1.15));
        QString apparent = service->apparentTemperature(location);
        if (!apparent.isEmpty())
            sky->addWidget(label(this, i18n("Feels like %1").arg(apparent), left, 1.0, false, true));
        sky->addStretch();
        now->addStretch();

        // the details: label above value, in columns
        QHBoxLayout *details = new QHBoxLayout(top, 3 * spacing);
        const char *names[] = { I18N_NOOP("Wind"), I18N_NOOP("Humidity"), I18N_NOOP("Pressure"),
                                I18N_NOOP("Next hour"), I18N_NOOP("Sun") };
        // "no rain" only when the next hours have amounts: unknown is not dry
        QString rain = service->rainNextHour(location);
        if (rain.isEmpty()) {
            QStringList next = service->hourlyValues(location, 2);
            bool known = next.count() == 2;
            for (QStringList::ConstIterator it = next.begin(); it != next.end(); ++it)
                if (field(QStringList::split(QChar('\t'), *it, true), 3).isEmpty())
                    known = false;
            if (known)
                rain = i18n("no rain");
        }
        QString rise = service->sunRiseTime(location);
        QString set = service->sunSetTime(location);
        QString values[] = {
            service->wind(location),
            service->relativeHumidity(location),
            service->pressure(location),
            rain,
            rise.isEmpty() || set.isEmpty() ? QString::null
                : i18n("sunrise - sunset", "%1 \xe2\x80\x93 %2").arg(rise).arg(set)
        };
        for (int i = 0; i < 5; ++i) {
            if (values[i].isEmpty())
                continue;
            QVBoxLayout *pair = new QVBoxLayout(details, 0);
            pair->addWidget(label(this, i18n(names[i]), left, 0.85, false, true));
            pair->addWidget(label(this, values[i], left));
        }
        details->addStretch();
        top->addWidget(separator(this));

        // the next 12 hours: equal columns
        QStringList hours = service->hourlyValues(location, hourColumns);
        if (!hours.isEmpty()) {
            QStringList first = QStringList::split(QChar('\t'), hours.first(), true);
            QLabel *caption = label(this, i18n("Next hours"), left, 1.0, true);
            QHBoxLayout *ch = new QHBoxLayout(top, spacing);
            ch->addWidget(caption);
            ch->addWidget(label(this, i18n("units of temperature and precipitation", "%1, %2")
                                    .arg(field(first, 6)).arg(field(first, 7)),
                                left, 0.85, false, true));
            ch->addStretch();
            QGridLayout *grid = new QGridLayout(top, 4, hourColumns, 0);
            grid->setRowSpacing(1, hourIcon + 4);
            int c = 0;
            for (QStringList::ConstIterator it = hours.begin(); it != hours.end() && c < hourColumns; ++it, ++c) {
                QStringList f = QStringList::split(QChar('\t'), *it, true);
                bool ok;
                QDateTime t = QDateTime::fromString(field(f, 0), Qt::ISODate);
                grid->addWidget(label(this, loc->formatTime(t.time()), center, 0.9, false, true), 0, c);
                grid->addWidget(iconLabel(this, field(f, 1), hourIcon, field(f, 5)), 1, c, Qt::AlignCenter);
                double temp = value(f, 2, &ok);
                grid->addWidget(label(this, ok ? degrees(temp) : QString::null, center, 1.0, true), 2, c);
                double p = value(f, 3, &ok);
                QString text = ok ? amount(p, field(f, 7)) : QString::null;
                QLabel *r;
                if (!text.isEmpty()) {
                    r = label(this, text, center, 0.85);
                    r->setPaletteForegroundColor(rainColor(this));
                } else {
                    double prob = value(f, 4, &ok);
                    r = label(this, ok && prob >= 10 ? i18n("%1 %").arg(long(prob)) : QString::null,
                              center, 0.85, false, true);
                }
                grid->addWidget(r, 3, c);
                grid->setColStretch(c, 1);
                grid->addColSpacing(c, column);
            }
            top->addWidget(separator(this));
        }

        // the next days: rows in columns, the temperatures within the week's
        QStringList days = service->dailyValues(location);
        if (!days.isEmpty()) {
            QStringList first = QStringList::split(QChar('\t'), days.first(), true);
            QHBoxLayout *ch = new QHBoxLayout(top, spacing);
            ch->addWidget(label(this, i18n("Next 7 days"), left, 1.0, true));
            ch->addWidget(label(this, i18n("units of temperature and precipitation", "%1, %2")
                                    .arg(field(first, 7)).arg(field(first, 8)),
                                left, 0.85, false, true));
            ch->addStretch();

            double weekMin = 1e9, weekMax = -1e9;
            for (QStringList::ConstIterator it = days.begin(); it != days.end(); ++it) {
                QStringList f = QStringList::split(QChar('\t'), *it, true);
                bool ok1, ok2;
                double hi = value(f, 2, &ok1), lo = value(f, 3, &ok2);
                if (ok1 && ok2) {
                    weekMin = QMIN(weekMin, lo);
                    weekMax = QMAX(weekMax, hi);
                }
            }

            // the gaps between the columns are in their widths: the rows
            // stay close together
            QGridLayout *grid = new QGridLayout(top, days.count(), 9, 2);
            grid->setColStretch(8, 1);
            const int degreesWidth = fm.width(QString::fromLatin1("-88")) + fm.width(QChar(0xb0));
            QDate today = QDate::fromString(service->localDate(location), Qt::ISODate);
            if (!today.isValid())
                today = QDate::currentDate();
            int row = 0;
            for (QStringList::ConstIterator it = days.begin(); it != days.end() && row < 7; ++it, ++row) {
                QStringList f = QStringList::split(QChar('\t'), *it, true);
                QDate date = QDate::fromString(field(f, 0), Qt::ISODate);
                QString day = date == today ? i18n("Today")
                            : date == today.addDays(1) ? i18n("Tomorrow")
                            : loc->calendar()->weekDayName(date, false);
                grid->addWidget(label(this, day, left), row, 0);
                grid->addWidget(iconLabel(this, field(f, 1), dayIcon, field(f, 6)), row, 1, Qt::AlignCenter);
                bool okHi, okLo, ok;
                double hi = value(f, 2, &okHi), lo = value(f, 3, &okLo);
                grid->addWidget(label(this, okLo ? degrees(lo) : QString::null, right, 1.0, false, true), row, 3);
                if (okHi && okLo) {
                    RangeBar *bar = new RangeBar(this, weekMin, weekMax, lo, hi);
                    bar->setFixedWidth(barWidth);
                    grid->addWidget(bar, row, 4, Qt::AlignCenter);
                }
                grid->addWidget(label(this, okHi ? degrees(hi) : QString::null, right, 1.0, true), row, 5);
                double p = value(f, 4, &ok);
                QString text = ok ? amount(p, field(f, 8)) : QString::null;
                QLabel *r = label(this, text, right);
                r->setPaletteForegroundColor(rainColor(this));
                grid->addWidget(r, row, 6);
                double prob = value(f, 5, &ok);
                grid->addWidget(label(this, ok && !text.isEmpty() ? i18n("%1 %").arg(long(prob))
                                                                  : QString::null,
                                      right, 0.85, false, true), row, 7);
            }
            grid->addColSpacing(0, fm.width(i18n("Wednesday")) + 2 * spacing);
            grid->addColSpacing(1, dayIcon + 2 * spacing);
            grid->addColSpacing(3, degreesWidth + spacing);
            grid->addColSpacing(4, barWidth + 2 * spacing);
            grid->addColSpacing(5, degreesWidth + spacing);
            grid->addColSpacing(6, fm.width(loc->formatNumber(88.8, 1)) + 3 * spacing);
            grid->addColSpacing(7, fm.width(QString::fromLatin1("100 %")) + 2 * spacing);
            top->addWidget(separator(this));
        }
        if (!error.isEmpty()) {
            QLabel *l = new QLabel(i18n("The last update failed: %1").arg(error), this);
            l->setAlignment(Qt::WordBreak);
            top->addWidget(l);
        }
    }

    top->addWidget(label(this, i18n("%1 \xc2\xb7 Weather data by Open-Meteo.com")
                                   .arg(service->modelName(location)),
                         left, 0.8, false, true));
}

void ForecastPopup::popup(const QRect &anchor, int direction)
{
    adjustSize();
    QSize s = size();
    QRect screen = QApplication::desktop()->screenGeometry(
        QApplication::desktop()->screenNumber(anchor.center()));
    QPoint p;
    switch (direction) {
    case KPanelApplet::Down:
        p = QPoint(anchor.left(), anchor.bottom() + 1);
        break;
    case KPanelApplet::Left:
        p = QPoint(anchor.left() - s.width(), anchor.top());
        break;
    case KPanelApplet::Right:
        p = QPoint(anchor.right() + 1, anchor.top());
        break;
    case KPanelApplet::Up:
    default:
        p = QPoint(anchor.left(), anchor.top() - s.height());
        break;
    }
    p.setX(QMAX(screen.left(), QMIN(p.x(), screen.right() + 1 - s.width())));
    p.setY(QMAX(screen.top(), QMIN(p.y(), screen.bottom() + 1 - s.height())));
    move(p);
    show();
}

void ForecastPopup::hideEvent(QHideEvent *e)
{
    QFrame::hideEvent(e);
    emit hidden();
}

void ForecastPopup::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape)
        close();
    else
        QFrame::keyPressEvent(e);
}

#include "forecastpopup.moc"
