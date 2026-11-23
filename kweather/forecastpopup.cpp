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
#include <qstringlist.h>
#include <qstylesheet.h>
#include <qtooltip.h>

#include <kcalendarsystem.h>
#include <kdialog.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <klocale.h>
#include <kpanelapplet.h>

#include "weathericons.h"
#include "weatherservice_stub.h"

namespace
{

QLabel *label(QWidget *parent, const QString &text, int align = Qt::AlignAuto)
{
    QLabel *l = new QLabel(text, parent);
    l->setAlignment(align | Qt::AlignVCenter | Qt::SingleLine);
    return l;
}

QLabel *iconLabel(QWidget *parent, const QString &icon, int size)
{
    QLabel *l = new QLabel(parent);
    l->setPixmap(WeatherIcons::load(icon, size));
    l->setAlignment(Qt::AlignCenter);
    return l;
}

QLabel *heading(QWidget *parent, const QString &text)
{
    QLabel *l = new QLabel(QString::fromLatin1("<b>") + QStyleSheet::escape(text)
                           + QString::fromLatin1("</b>"), parent);
    return l;
}

QString field(const QStringList &fields, uint i)
{
    return i < fields.count() ? fields[i] : QString::null;
}

QString escape(const QString &s)
{
    return QStyleSheet::escape(s);
}

}

ForecastPopup::ForecastPopup(WeatherService_stub *service,
                             const QString &location, QWidget *parent,
                             const char *name)
    : QFrame(parent, name, WType_Popup | WDestructiveClose)
{
    setFrameStyle(QFrame::PopupPanel | QFrame::Raised);
    setLineWidth(1);
    setMargin(0);

    QVBoxLayout *top = new QVBoxLayout(this, KDialog::marginHint(),
                                       KDialog::spacingHint());

    QString name_ = service->stationName(location);
    QString country = service->stationCountry(location);
    QString title = country.isEmpty() ? name_ : i18n("place, country", "%1, %2")
                                                      .arg(name_).arg(country);
    QLabel *titleLabel = new QLabel(QString::fromLatin1("<qt><nobr><big><b>")
                                    + escape(title)
                                    + QString::fromLatin1("</b></big></nobr></qt>"), this);
    top->addWidget(titleLabel);

    QString error = service->error(location);
    QString temperature = service->temperature(location);

    if (temperature.isEmpty()) {
        top->addWidget(label(this, error.isEmpty()
                             ? i18n("Waiting for the weather data...")
                             : i18n("The weather could not be fetched: %1").arg(error)));
    } else {
        // the current weather
        QHBoxLayout *now = new QHBoxLayout(top);
        now->addWidget(iconLabel(this, service->currentIconString(location), 64));
        QStringList weather = service->weather(location);
        QString text = QString::fromLatin1("<qt><nobr><font size=\"+3\"><b>")
                     + escape(temperature) + QString::fromLatin1("</b></font> &nbsp;")
                     + escape(weather.join(QString::fromLatin1(", "))) + QString::fromLatin1("</nobr>");
        QStringList details;
        QString apparent = service->apparentTemperature(location);
        if (!apparent.isEmpty() && apparent != temperature)
            details.append(i18n("Feels like %1").arg(apparent));
        QString wind = service->wind(location);
        if (!wind.isEmpty())
            details.append(i18n("Wind: %1").arg(wind));
        QString line2 = details.join(QString::fromLatin1(" \xb7 "));
        details.clear();
        QString humidity = service->relativeHumidity(location);
        if (!humidity.isEmpty())
            details.append(i18n("Humidity: %1").arg(humidity));
        QString pressure = service->pressure(location);
        if (!pressure.isEmpty())
            details.append(i18n("Pressure: %1").arg(pressure));
        QString line3 = details.join(QString::fromLatin1(" \xb7 "));
        details.clear();
        QString rise = service->sunRiseTime(location);
        QString set = service->sunSetTime(location);
        if (!rise.isEmpty() && !set.isEmpty())
            details.append(i18n("Sunrise %1, sunset %2").arg(rise).arg(set));
        QString line4 = details.join(QString::null);
        text += QString::fromLatin1("<br><nobr>") + escape(line2)
              + QString::fromLatin1("</nobr><br><nobr>") + escape(line3)
              + QString::fromLatin1("</nobr><br><nobr>") + escape(line4)
              + QString::fromLatin1("</nobr></qt>");
        now->addWidget(new QLabel(text, this), 1);

        // the next hours
        QStringList hours = service->hourlyForecast(location, 12);
        if (!hours.isEmpty()) {
            top->addWidget(heading(this, i18n("Next hours")));
            QGridLayout *grid = new QGridLayout(top, 4, hours.count(), 2);
            int col = 0;
            for (QStringList::ConstIterator it = hours.begin(); it != hours.end(); ++it, ++col) {
                QStringList f = QStringList::split(QChar('\t'), *it, true);
                QDateTime t = QDateTime::fromString(field(f, 0), Qt::ISODate);
                grid->addWidget(label(this, KGlobal::locale()->formatTime(t.time()),
                                      Qt::AlignHCenter), 0, col);
                QLabel *icon = iconLabel(this, field(f, 1), 22);
                grid->addWidget(icon, 1, col);
                grid->addWidget(label(this, field(f, 2), Qt::AlignHCenter), 2, col);
                QString rain = field(f, 3);
                if (rain.isEmpty() && !field(f, 4).isEmpty() && field(f, 4) != i18n("%1 %").arg(0))
                    rain = field(f, 4);
                QLabel *r = label(this, rain, Qt::AlignHCenter);
                QFont small = r->font();
                small.setPointSize(QMAX(6, small.pointSize() - 1));
                r->setFont(small);
                grid->addWidget(r, 3, col);
                QString tip = field(f, 5);
                if (!field(f, 4).isEmpty())
                    tip += QString::fromLatin1(", ") + i18n("probability of precipitation %1").arg(field(f, 4));
                QToolTip::add(icon, tip);
            }
        }

        // the next days
        QStringList days = service->dailyForecast(location);
        if (!days.isEmpty()) {
            top->addWidget(heading(this, i18n("Next 7 days")));
            QGridLayout *grid = new QGridLayout(top, days.count(), 6, 2);
            grid->setColStretch(5, 1);
            int row = 0;
            QDate today = QDate::currentDate();
            for (QStringList::ConstIterator it = days.begin(); it != days.end(); ++it, ++row) {
                QStringList f = QStringList::split(QChar('\t'), *it, true);
                QDate date = QDate::fromString(field(f, 0), Qt::ISODate);
                QString day = date == today ? i18n("Today")
                            : date == today.addDays(1) ? i18n("Tomorrow")
                            : KGlobal::locale()->calendar()->weekDayName(date, false);
                grid->addWidget(label(this, day), row, 0);
                grid->addWidget(iconLabel(this, field(f, 1), 22), row, 1);
                grid->addWidget(label(this, QString::fromLatin1("<b>") + escape(field(f, 2))
                                      + QString::fromLatin1("</b>"), Qt::AlignRight), row, 2);
                grid->addWidget(label(this, field(f, 3), Qt::AlignRight), row, 3);
                QString rain = field(f, 4);
                if (!field(f, 5).isEmpty() && !rain.isEmpty())
                    rain += QString::fromLatin1(" (") + field(f, 5) + QChar(')');
                grid->addWidget(label(this, rain, Qt::AlignRight), row, 4);
                grid->addWidget(label(this, field(f, 6)), row, 5);
            }
            grid->setColSpacing(2, 40);
            grid->setColSpacing(4, 60);
        }
        if (!error.isEmpty())
            top->addWidget(label(this, i18n("The last update failed: %1").arg(error)));
    }

    QString footer = i18n("%1. Weather data by Open-Meteo.com.")
                         .arg(service->modelName(location));
    QString date = service->date(location);
    if (!date.isEmpty())
        footer = i18n("%1 (local time). %2").arg(date).arg(footer);
    QLabel *f = new QLabel(QString::fromLatin1("<qt><small>") + escape(footer)
                           + QString::fromLatin1("</small></qt>"), this);
    top->addWidget(f);
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
