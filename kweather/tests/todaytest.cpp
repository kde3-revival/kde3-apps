/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

// kweathertodaytest [--png DIR]: the Today card of the forecast
// (WeatherLib::dailyValues(), the first day) from canned answers of
// Open-Meteo, made around the clock of now at a place where it is the
// time of day of each case (a fixed UTC offset).  No network: the answers
// are given with WeatherLib::setAnswer().  With --png (needs DISPLAY),
// also a picture of the current icon and the cards of each case.

#include <qapplication.h>
#include <qdatetime.h>
#include <qlocale.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qstringlist.h>
#include <qvaluelist.h>

#include <kglobal.h>
#include <kinstance.h>
#include <klocale.h>

#include "forecastchart.h"
#include "weathericons.h"
#include "weatherlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

namespace
{

const char *const location = "45.0000,25.0000,ecmwf_ifs";
const int hoursInAnswer = 72;    // from local midnight today
const int daysInAnswer = 3;

struct Hour
{
    double temp;
    double precip;
    int prob;
    int code;
    bool day;
};

struct Day
{
    double tmax, tmin, precip;
    int prob;
    int code;
};

struct Case
{
    const char *name;
    int localTime;              // seconds after local midnight, now
    double currentTemp;
    int currentCode;
    bool currentDay;
    Hour hours[hoursInAnswer];
    Day days[daysInAnswer];

    // local midnight of today and the UTC offset, set by answer()
    long midnight;
    long offset;
};

QString num(double v)
{
    return QLocale::c().toString(v, 'g', 10);
}

/** Open-Meteo's answer (timeformat=unixtime) for @p c, now = @p t. */
QByteArray answer(Case &c, long t)
{
    // the offset that makes it c.localTime there, in whole minutes
    long off = (c.localTime - t % 86400) % 86400;
    if (off > 43200)
        off -= 86400;
    else if (off <= -43200)
        off += 86400;
    off = off / 60 * 60;
    c.offset = off;
    long local = t + off;
    c.midnight = local - local % 86400 - off;

    QString s = QString::fromLatin1("{\"latitude\":45.0,\"longitude\":25.0,"
                                    "\"timezone\":\"Etc/Nowhere\",\"utc_offset_seconds\":")
              + QString::number(off);
    s += QString::fromLatin1(",\"current\":{\"time\":") + QString::number(t - t % 900)
       + QString::fromLatin1(",\"temperature_2m\":") + num(c.currentTemp)
       + QString::fromLatin1(",\"weather_code\":") + QString::number(c.currentCode)
       + QString::fromLatin1(",\"is_day\":") + QString::number(c.currentDay ? 1 : 0)
       + QString::fromLatin1(",\"precipitation\":0}");

    QStringList time, temp, precip, prob, code, day;
    for (int i = 0; i < hoursInAnswer; ++i) {
        const Hour &h = c.hours[i];
        time.append(QString::number(c.midnight + i * 3600L));
        temp.append(num(h.temp));
        precip.append(num(h.precip));
        prob.append(QString::number(h.prob));
        code.append(QString::number(h.code));
        day.append(QString::number(h.day ? 1 : 0));
    }
    const QString comma = QString::fromLatin1(",");
    s += QString::fromLatin1(",\"hourly\":{\"time\":[") + time.join(comma)
       + QString::fromLatin1("],\"temperature_2m\":[") + temp.join(comma)
       + QString::fromLatin1("],\"precipitation\":[") + precip.join(comma)
       + QString::fromLatin1("],\"precipitation_probability\":[") + prob.join(comma)
       + QString::fromLatin1("],\"weather_code\":[") + code.join(comma)
       + QString::fromLatin1("],\"is_day\":[") + day.join(comma) + QString::fromLatin1("]}");

    QStringList dtime, tmax, tmin, dprecip, dprob, dcode;
    for (int i = 0; i < daysInAnswer; ++i) {
        const Day &d = c.days[i];
        dtime.append(QString::number(c.midnight + i * 86400L));
        tmax.append(num(d.tmax));
        tmin.append(num(d.tmin));
        dprecip.append(num(d.precip));
        dprob.append(QString::number(d.prob));
        dcode.append(QString::number(d.code));
    }
    s += QString::fromLatin1(",\"daily\":{\"time\":[") + dtime.join(comma)
       + QString::fromLatin1("],\"weather_code\":[") + dcode.join(comma)
       + QString::fromLatin1("],\"temperature_2m_max\":[") + tmax.join(comma)
       + QString::fromLatin1("],\"temperature_2m_min\":[") + tmin.join(comma)
       + QString::fromLatin1("],\"precipitation_sum\":[") + dprecip.join(comma)
       + QString::fromLatin1("],\"precipitation_probability_max\":[") + dprob.join(comma)
       + QString::fromLatin1("]}}");
    // as downloaded: without the 0 of a QCString
    QCString text = s.latin1();
    QByteArray bytes;
    bytes.duplicate(text.data(), text.length());
    return bytes;
}

/** A day: dry and clear until sunrise at 7, rainy in the morning
    (6-12), cloudy after, night from 17; temperatures 1 to 8. */
void plainDay(Case &c)
{
    for (int i = 0; i < hoursInAnswer; ++i) {
        int hour = i % 24;
        Hour &h = c.hours[i];
        h.temp = hour < 14 ? 1 + hour * 0.5 : 8 - (hour - 14) * 0.5;
        h.precip = 0;
        h.prob = 5;
        h.code = hour <= 6 || hour >= 17 ? 0 : 2;
        h.day = hour >= 7 && hour < 17;
        if (hour >= 6 && hour <= 12) {
            h.precip = 0.8;
            h.prob = 80;
            h.code = 63;
        }
    }
    for (int i = 0; i < daysInAnswer; ++i) {
        Day &d = c.days[i];
        d.tmax = 8;
        d.tmin = 1;
        d.precip = 5.6;
        d.prob = 80;
        d.code = 63;
    }
}

int failures = 0;

/** Gives @p lib the answer for @p c. */
void give(WeatherLib &lib, Case &c, long t)
{
    if (!lib.setAnswer(location, answer(c, t))) {
        ++failures;
        printf("FAIL %s: answer not taken: %s\n", c.name, lib.error(location).latin1());
    }
}

void check(const char *name, const char *what, const QString &got, const QString &want)
{
    bool ok = got == want;
    if (!ok)
        ++failures;
    printf("%s %s %s: %s%s%s\n", ok ? "PASS" : "FAIL", name, what, got.latin1(),
           ok ? "" : ", expected ", ok ? "" : want.latin1());
}

QString field(const QString &line, int i)
{
    return QStringList::split(QChar('\t'), line, true)[i];
}

/** The fields of the first day: icon, maximum, minimum, precipitation,
    probability, description. */
void checkToday(const Case &c, const QStringList &days, const char *icon,
                const char *tmax, const char *tmin, const char *precip,
                const char *prob, const char *description)
{
    if (days.count() < 2) {
        ++failures;
        printf("FAIL %s: %d days\n", c.name, int(days.count()));
        return;
    }
    QString today = days[0];
    check(c.name, "date", field(today, 0),
          QDate(1970, 1, 1).addDays((c.midnight + c.offset) / 86400).toString(Qt::ISODate));
    check(c.name, "icon", field(today, 1), QString::fromLatin1(icon));
    check(c.name, "maximum", field(today, 2), QString::fromLatin1(tmax));
    check(c.name, "minimum", field(today, 3), QString::fromLatin1(tmin));
    check(c.name, "precipitation", field(today, 4), QString::fromLatin1(precip));
    check(c.name, "probability", field(today, 5), QString::fromLatin1(prob));
    check(c.name, "description", field(today, 6), QString::fromLatin1(description));
    // the other days: the daily values of Open-Meteo, as before
    const Day &d = c.days[1];
    QString tomorrow = days[1];
    check(c.name, "tomorrow", field(tomorrow, 1) + '/' + field(tomorrow, 2) + '/'
                              + field(tomorrow, 3) + '/' + field(tomorrow, 4),
          WeatherIcons::iconName(d.code, true) + '/' + QLocale::c().toString(d.tmax, 'f', 1)
          + '/' + QLocale::c().toString(d.tmin, 'f', 1) + '/'
          + QLocale::c().toString(d.precip, 'f', 2));
}

/** The current icon and the cards, as the applet and the popup show them. */
void picture(WeatherLib &lib, const Case &c, const QString &dir)
{
    QValueList<ForecastDay> list;
    QStringList days = lib.dailyValues(location);
    for (QStringList::ConstIterator it = days.begin(); it != days.end(); ++it) {
        QStringList f = QStringList::split(QChar('\t'), *it, true);
        ForecastDay d;
        d.date = QDate::fromString(f[0], Qt::ISODate);
        d.icon = f[1];
        d.description = f[6];
        d.high = QLocale::c().toDouble(f[2]);
        d.low = QLocale::c().toDouble(f[3]);
        d.hasTemperature = true;
        double p = QLocale::c().toDouble(f[4]);
        d.precipitation = p >= 0.05 ? KGlobal::locale()->formatNumber(p, 1) + QString::fromLatin1(" mm")
                                    : QString::null;
        list.append(d);
    }
    DayStrip strip(0, 3 * 90, 32);
    strip.setDays(list, list.isEmpty() ? QDate() : list.first().date);
    strip.setCurrent(0);
    strip.show();
    qApp->processEvents();
    QPixmap cards = QPixmap::grabWidget(&strip);

    QFontMetrics fm(strip.font());
    QString now = QString::fromLatin1("%1  now %2 (%3)")
                      .arg(QString::fromLatin1(c.name))
                      .arg(QTime(0, 0).addSecs(c.localTime).toString(QString::fromLatin1("hh:mm")))
                      .arg(lib.temperature(location));
    int w = QMAX(cards.width(), 40 + fm.width(now) + 8);
    QPixmap out(w, 40 + cards.height());
    out.fill(strip.paletteBackgroundColor());
    QPainter p(&out);
    p.drawPixmap(4, 4, WeatherIcons::load(lib.iconName(location), 32));
    p.setPen(strip.paletteForegroundColor());
    p.drawText(40, 4, w - 44, 32, Qt::AlignVCenter | Qt::AlignLeft, now);
    p.drawPixmap(0, 40, cards);
    p.end();
    out.save(dir + '/' + QString::fromLatin1(c.name) + QString::fromLatin1(".png"), "PNG");
}

}

int main(int argc, char **argv)
{
    QString pngDir;
    if (argc == 3 && strcmp(argv[1], "--png") == 0)
        pngDir = QString::fromLocal8Bit(argv[2]);

    // never the user's configuration
    char kdehome[] = "/tmp/kweathertodaytest-XXXXXX";
    if (!mkdtemp(kdehome)) {
        perror("mkdtemp");
        return 2;
    }
    setenv("KDEHOME", kdehome, 1);
    setenv("KDE_LANG", "C", 1);

    // no KApplication: it would start a DCOP server
    KInstance *instance = new KInstance("kweathertodaytest");
    QApplication *app = 0;
    if (!pngDir.isEmpty()) {
        static int args = 1;
        app = new QApplication(args, argv);
    }

    WeatherLib lib;
    long t = static_cast<long>(time(0));

    // 1: 20:30, a rainy morning, nothing to come: no rain icon, no
    // amount; the night icon as that of now; 21:00 to 24:00 and now
    {
        static Case c;
        c.name = "evening";
        c.localTime = 20 * 3600 + 30 * 60;
        c.currentTemp = 2.4;
        c.currentCode = 0;
        c.currentDay = false;
        plainDay(c);
        c.hours[22].code = 1;
        give(lib, c, t);
        QStringList days = lib.dailyValues(location);
        check(c.name, "now", lib.iconName(location), QString::fromLatin1("weather-clear-night"));
        // hours 21-24: 4.5, 4.0, 3.5, 1.0 (midnight of the next day)
        checkToday(c, days, "weather-few-clouds-night", "4.5", "1.0", "0.00", "5",
                   "Mainly clear");
        if (!pngDir.isEmpty())
            picture(lib, c, pngDir);
    }

    // 2: 14:10, snow codes with 0 mm to come (and 0.02 mm, not shown):
    // overcast, not snow; showers without an amount: partly cloudy
    {
        static Case c;
        c.name = "snow-dry";
        c.localTime = 14 * 3600 + 10 * 60;
        c.currentTemp = -1;
        c.currentCode = 3;
        c.currentDay = true;
        plainDay(c);
        for (int i = 15; i <= 24; ++i) {
            c.hours[i].temp = -2;
            c.hours[i].code = i < 19 ? 71 : 3;
            c.hours[i].precip = i == 16 ? 0.02 : 0;
        }
        c.days[0].code = 73;
        give(lib, c, t);
        checkToday(c, lib.dailyValues(location), "weather-overcast", "-1.0", "-2.0", "0.02",
                   "5", "Overcast");

        c.name = "showers-dry";
        for (int i = 15; i <= 24; ++i) {
            c.hours[i].code = i < 19 ? 85 : 1;
            c.hours[i].precip = 0;
        }
        give(lib, c, t);
        checkToday(c, lib.dailyValues(location), "weather-clouds", "-1.0", "-2.0", "0.00",
                   "5", "Partly cloudy");

        // the control: snow to come
        c.name = "snow-wet";
        for (int i = 15; i <= 24; ++i) {
            c.hours[i].code = i < 19 ? 71 : 3;
            c.hours[i].precip = i == 16 ? 0.3 : 0;
        }
        give(lib, c, t);
        checkToday(c, lib.dailyValues(location), "weather-snow", "-1.0", "-2.0", "0.30",
                   "5", "Light snow");
        if (!pngDir.isEmpty()) {
            c.name = "snow-dry";
            for (int i = 15; i <= 24; ++i)
                c.hours[i].precip = 0;
            give(lib, c, t);
            picture(lib, c, pngDir);
        }
    }

    // 3: 23:20, the last hour: it and the first two of tomorrow (at
    // least 3), not the rain of the hour after them
    {
        static Case c;
        c.name = "last-hour";
        c.localTime = 23 * 3600 + 20 * 60;
        c.currentTemp = 0.6;
        c.currentCode = 0;
        c.currentDay = false;
        plainDay(c);
        c.hours[24].temp = 0;
        c.hours[24].code = 0;
        c.hours[25].temp = -1;
        c.hours[25].code = 2;
        c.hours[26].temp = -2;
        c.hours[26].code = 3;
        c.hours[27].temp = -5;
        c.hours[27].code = 61;
        c.hours[27].precip = 1;
        c.hours[27].prob = 90;
        give(lib, c, t);
        checkToday(c, lib.dailyValues(location), "weather-overcast", "0.6", "-2.0", "0.00", "5",
                   "Overcast");
        if (!pngDir.isEmpty())
            picture(lib, c, pngDir);
    }

    // 4: 05:30, before sunrise: the day icon, the day is still to come;
    // the rain of the morning too
    {
        static Case c;
        c.name = "before-sunrise";
        c.localTime = 5 * 3600 + 30 * 60;
        c.currentTemp = 3.5;
        c.currentCode = 0;
        c.currentDay = false;
        plainDay(c);
        give(lib, c, t);
        checkToday(c, lib.dailyValues(location), "weather-showers", "8.0", "1.0", "5.60", "80",
                   "Rain");
        // dry: the clouds of the day, not of the night as now
        c.name = "before-sunrise-dry";
        for (int i = 0; i < hoursInAnswer; ++i) {
            if (c.hours[i].code == 63)
                c.hours[i].code = 2;
            c.hours[i].precip = 0;
            c.hours[i].prob = 5;
        }
        give(lib, c, t);
        checkToday(c, lib.dailyValues(location), "weather-clouds", "8.0", "1.0", "0.00", "5",
                   "Partly cloudy");
        if (!pngDir.isEmpty())
            picture(lib, c, pngDir);
    }

    // the text of dailyForecast(): the same day
    {
        QStringList days = lib.daily(location);
        check("before-sunrise-dry", "text", days.isEmpty() ? QString::null
                                            : field(days[0], 1) + '/' + field(days[0], 4),
              QString::fromLatin1("weather-clouds/"));
    }

    printf("%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
    delete app;
    delete instance;
    rmdir(kdehome);
    return failures ? 1 : 0;
}
