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
#include <qscrollbar.h>
#include <qscrollview.h>
#include <qtabbar.h>
#include <qtooltip.h>

#include <kcalendarsystem.h>
#include <kdialog.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <klocale.h>
#include <kpanelapplet.h>
#include <kseparator.h>

#include "forecastchart.h"
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

QWidget *separator(QWidget *parent)
{
    return new KSeparator(KSeparator::HLine, parent);
}

}

ForecastPopup::ForecastPopup(WeatherService_stub *service,
                             const QString &location, bool horizontalPanel,
                             QWidget *parent, const char *name)
    : QFrame(parent, name, WType_Popup), m_scroll(0), m_content(0), m_service(service),
      m_location(location), m_chart(0), m_strip(0), m_tabs(0), m_dayCaption(0),
      m_day(0), m_kind(HourChart::Temperature)
{
    setFrameStyle(QFrame::PopupPanel | QFrame::Raised);
    setLineWidth(1);
    setMargin(0);
    QVBoxLayout *outer = new QVBoxLayout(this, frameWidth(), 0);
    // scrolls only when even the most compact contents are too high
    m_scroll = new QScrollView(this);
    m_scroll->setFrameStyle(QFrame::NoFrame);
    m_scroll->setHScrollBarMode(QScrollView::AlwaysOff);
    m_scroll->setVScrollBarMode(QScrollView::AlwaysOff);
    m_scroll->setFocusPolicy(NoFocus);
    m_scroll->viewport()->setFocusPolicy(NoFocus);
    m_scroll->viewport()->setBackgroundMode(PaletteBackground);
    outer->addWidget(m_scroll);

    // everything if it fits the work area of the screen, else less, step
    // by step (see build())
    QDesktopWidget *desktop = QApplication::desktop();
    QRect area = desktop->availableGeometry(desktop->screenNumber(parent));
    int space = area.height();
    // beside a horizontal panel: the room above or below the applet (the
    // work area may still include the panel)
    if (horizontalPanel && parent) {
        QRect screen = desktop->screenGeometry(desktop->screenNumber(parent));
        QRect anchor(parent->mapToGlobal(QPoint(0, 0)), parent->size());
        space = QMIN(space, QMAX(anchor.top() - screen.top(), screen.bottom() - anchor.bottom()));
    }
    int room = space - 2 * frameWidth();
    int h = 0;
    for (int level = 0; ; ++level) {
        build(service, location, level);
        h = m_content->sizeHint().height();
        if (h <= room || level >= maxLevel)
            break;
    }
    int contentWidth = width() - 2 * frameWidth();
    m_content->resize(contentWidth, h);
    if (h <= room) {
        m_scroll->setFixedSize(contentWidth, h);
    } else {
        int bar = m_scroll->verticalScrollBar()->sizeHint().width();
        m_scroll->setVScrollBarMode(QScrollView::AlwaysOn);
        setFixedWidth(QMIN(width() + bar, area.width()));
        m_scroll->setFixedSize(width() - 2 * frameWidth(), room);
    }
}

/** The contents; from level 1 on, the more compact: a smaller current
    weather (1), a lower chart (2), no details (3), smaller icons on the
    cards (4), a still lower chart (5). */
void ForecastPopup::build(WeatherService_stub *service, const QString &location,
                          int level)
{
    delete m_content;
    m_chart = 0;
    m_strip = 0;
    m_tabs = 0;
    m_dayCaption = 0;
    m_content = new QWidget(m_scroll->viewport());
    m_content->setBackgroundMode(PaletteBackground);
    m_scroll->addChild(m_content);
    QWidget *w = m_content;

    const int margin = KDialog::marginHint();
    const int spacing = KDialog::spacingHint();
    const int right = Qt::AlignRight | Qt::AlignVCenter;
    const int left = Qt::AlignAuto | Qt::AlignVCenter;
    KLocale *loc = KGlobal::locale();

    QVBoxLayout *top = new QVBoxLayout(w, margin, spacing);

    // the width: that of 12 equal columns of the hours
    QFontMetrics fm(font());
    int column = QMAX(QMAX(fm.width(QString::fromLatin1("00:00")), fm.width(QString::fromLatin1("-88")) + fm.width(QChar(0xb0))),
                      QMAX(hourIcon, fm.width(i18n("less than an amount", "< %1").arg(loc->formatNumber(0.1, 1)))));
    column += 2 * spacing;
    // at least 36 average characters: room for the details row
    column = QMAX(column, (36 * fm.width(QChar('x')) + 11) / 12);
    // 12 hours, or fewer on a narrow screen or with a large font
    QRect screen = QApplication::desktop()->screenGeometry(parentWidget());
    int hourColumns = (screen.width() - 2 * margin - 2 * frameWidth() - 16) / column;
    hourColumns = QMAX(4, QMIN(12, hourColumns));
    const int popupWidth = QMIN(hourColumns * column + 2 * margin + 2 * frameWidth(), screen.width());
    setFixedWidth(popupWidth);
    const int contentWidth = popupWidth - 2 * margin - 2 * frameWidth();

    // smaller when the screen is too short for everything
    const bool compact = level >= 1;
    const int chartHeight = (level >= 5 ? 4 : level >= 2 ? 6 : 9) * fm.height();
    const bool withDetails = level < 3;
    const int cardIcon = level >= 4 ? 22 : dayIcon;

    // the place, and when the data is from
    QHBoxLayout *header = new QHBoxLayout(top, spacing);
    QString place = service->stationName(location);
    QString country = service->stationCountry(location);
    header->addWidget(label(w, country.isEmpty() ? place
                            : i18n("place, country", "%1, %2").arg(place).arg(country),
                            left, 1.2, true));
    header->addStretch();
    QDateTime updated = QDateTime::fromString(service->currentTime(location), Qt::ISODate);
    if (updated.isValid())
        header->addWidget(label(w, i18n("Updated %1").arg(loc->formatTime(updated.time())),
                                right | Qt::AlignBottom, 0.85, false, true));
    top->addWidget(separator(w));

    QString error = service->error(location);
    QString temperature = service->temperature(location);
    if (temperature.isEmpty()) {
        QLabel *l = new QLabel(error.isEmpty() ? i18n("Waiting for the weather data...")
                               : i18n("The weather could not be fetched: %1").arg(error), w);
        l->setAlignment(Qt::AlignCenter | Qt::WordBreak);
        l->setMinimumHeight(4 * fm.height());
        top->addWidget(l);
    } else {
        // now: the icon, the temperature, the sky and how it feels
        QHBoxLayout *now = new QHBoxLayout(top, 2 * spacing);
        now->addWidget(iconLabel(w, service->currentIconString(location),
                                     compact ? 32 : nowIcon, QString::null));
        now->addWidget(label(w, temperature, left, compact ? 1.6 : 2.4, true));
        QVBoxLayout *sky = new QVBoxLayout(now, 0);
        sky->addStretch();
        sky->addWidget(label(w, service->weather(location).join(QString::fromLatin1(", ")),
                             left, compact ? 1.0 : 1.15));
        QString apparent = service->apparentTemperature(location);
        if (!apparent.isEmpty())
            sky->addWidget(label(w, i18n("Feels like %1").arg(apparent), left, 1.0, false, true));
        sky->addStretch();
        now->addStretch();

        // the details: label above value, in columns
        QHBoxLayout *details = withDetails ? new QHBoxLayout(top, 3 * spacing) : 0;
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
        for (int i = 0; details && i < 5; ++i) {
            if (values[i].isEmpty())
                continue;
            QVBoxLayout *pair = new QVBoxLayout(details, 0);
            pair->addWidget(label(w, i18n(names[i]), left, 0.85, false, true));
            pair->addWidget(label(w, values[i], left));
        }
        if (details) {
            details->addStretch();
            top->addWidget(separator(w));
        }

        // the hours of the selected day as a chart: temperature,
        // precipitation or wind
        QHBoxLayout *ch = new QHBoxLayout(top, spacing);
        m_tabs = new QTabBar(w);
        m_tabs->setFocusPolicy(NoFocus);
        m_tabs->addTab(new QTab(i18n("Temperature")));
        m_tabs->addTab(new QTab(i18n("Precipitation")));
        m_tabs->addTab(new QTab(i18n("Wind")));
        m_tabs->setCurrentTab(m_kind);
        connect(m_tabs, SIGNAL(selected(int)), SLOT(slotKind(int)));
        ch->addWidget(m_tabs, 0, Qt::AlignBottom);
        ch->addStretch();
        m_dayCaption = label(w, QString::null, right | Qt::AlignBottom, 0.85, false, true);
        ch->addWidget(m_dayCaption);
        m_chart = new HourChart(w, chartHeight);
        m_chart->setKind(m_kind);
        top->addWidget(m_chart);

        // the days, as cards to choose the day of the chart
        QStringList days = service->dailyValues(location);
        QValueList<ForecastDay> list;
        m_dates.clear();
        for (QStringList::ConstIterator it = days.begin(); it != days.end(); ++it) {
            QStringList f = QStringList::split(QChar('\t'), *it, true);
            ForecastDay d;
            d.date = QDate::fromString(field(f, 0), Qt::ISODate);
            d.icon = field(f, 1);
            d.description = field(f, 6);
            bool okHi, okLo, ok;
            d.high = value(f, 2, &okHi);
            d.low = value(f, 3, &okLo);
            d.hasTemperature = okHi && okLo;
            double p = value(f, 4, &ok);
            QString a = ok ? amount(p, field(f, 8)) : QString::null;
            d.precipitation = a.isEmpty() ? a : a + QChar(' ') + field(f, 8);
            list.append(d);
            m_dates.append(d.date);
        }
        m_today = QDate::fromString(service->localDate(location), Qt::ISODate);
        if (!m_today.isValid())
            m_today = QDate::currentDate();
        m_strip = new DayStrip(w, contentWidth, cardIcon);
        m_strip->setDays(list, m_today);
        m_day = QMAX(0, QMIN(m_day, int(list.count()) - 1));
        m_strip->setCurrent(m_day);
        connect(m_strip, SIGNAL(selected(int)), SLOT(slotDay(int)));
        top->addWidget(m_strip);
        top->addWidget(separator(w));
        showDay();

        if (!error.isEmpty()) {
            QLabel *l = new QLabel(i18n("The last update failed: %1").arg(error), w);
            l->setAlignment(Qt::WordBreak);
            top->addWidget(l);
        }
    }

    top->addWidget(label(w, i18n("%1 \xc2\xb7 Weather data by Open-Meteo.com")
                                   .arg(service->modelName(location)),
                         left, 0.8, false, true));

    w->show();
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

void ForecastPopup::showDay()
{
    if (!m_chart || m_dates.isEmpty())
        return;
    QDate date = m_dates[QMIN(m_day, int(m_dates.count()) - 1)];
    bool today = date == m_today;
    m_chart->setHours(m_service->dayHours(m_location,
                                          today ? QString::null : date.toString(Qt::ISODate)));
    updateCaption();
}

void ForecastPopup::updateCaption()
{
    if (!m_dayCaption || m_dates.isEmpty())
        return;
    QDate date = m_dates[QMIN(m_day, int(m_dates.count()) - 1)];
    QString day = date == m_today ? i18n("Next 24 hours")
                : KGlobal::locale()->formatDate(date, false);
    QString unit = m_chart->unit();
    m_dayCaption->setText(unit.isEmpty() ? day : i18n("day and unit", "%1 \xc2\xb7 %2").arg(day).arg(unit));
}

void ForecastPopup::slotDay(int index)
{
    m_day = index;
    showDay();
}

void ForecastPopup::slotKind(int kind)
{
    m_kind = kind;
    if (m_chart)
        m_chart->setKind(kind);
    updateCaption();
}

void ForecastPopup::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Escape:
        close();
        break;
    case Qt::Key_Left:
    case Qt::Key_Right:
        // the previous or next day
        if (m_strip && m_strip->count()) {
            int day = m_strip->current() + (e->key() == Qt::Key_Left ? -1 : 1);
            m_strip->setCurrent(day);
            slotDay(m_strip->current());
        }
        break;
    case Qt::Key_Up:
    case Qt::Key_Down:
        // the previous or next chart
        if (m_tabs) {
            int kind = (m_kind + (e->key() == Qt::Key_Up ? 2 : 1)) % 3;
            m_tabs->setCurrentTab(kind);
            slotKind(kind);
        }
        break;
    default:
        QFrame::keyPressEvent(e);
    }
}

#include "forecastpopup.moc"
