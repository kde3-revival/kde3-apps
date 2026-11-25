/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

// kweathertodaytest [--png DIR]: the Today card of the forecast
// (WeatherLib::dailyValues(), the first day) from canned answers of
// Open-Meteo for Bucharest (Europe/Bucharest, from the zoneinfo of the
// system), at fixed times (WeatherLib::setClock()).  No network: the
// answers are given with WeatherLib::setAnswer().  With --png (needs
// DISPLAY), also a picture of the current icon and the cards of some
// cases.

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
#include <unistd.h>

namespace
{

const QString location = QString::fromLatin1("44.4323,26.1063,ecmwf_ifs");
const QString hybrid = QString::fromLatin1("44.4323,26.1063,hybrid");

const double none = -999999;   // null in the answer

struct Row
{
    long time;
    double temp, precip, prob;
    int code;       // -1: null
    int light;      // is_day, -1: null
};

struct DayRow
{
    long time;
    double tmax, tmin, precip, prob;
    int code;
    long sunrise, sunset;   // 0: null
};

struct Answer
{
    QString zone;
    long offset;            // utc_offset_seconds (at the time of the answer)
    bool current;
    long currentTime;
    double currentTemp;
    int currentCode;
    int currentLight;
    QValueList<Row> rows;
    QValueList<DayRow> days;
    int precipCount;        // the length of "precipitation", -1: all
};

/** The unix time of a UTC date and time. */
long utc(int y, int m, int d, int h, int min)
{
    return QDate(1970, 1, 1).daysTo(QDate(y, m, d)) * 86400L + h * 3600L + min * 60L;
}

QString num(double v)
{
    return v == none ? QString::fromLatin1("null") : QLocale::c().toString(v, 'g', 10);
}

QString integer(long v, bool null)
{
    return null ? QString::fromLatin1("null") : QString::number(v);
}

QByteArray json(const Answer &a)
{
    const QString comma = QString::fromLatin1(",");
    QString s = QString::fromLatin1("{\"latitude\":44.43,\"longitude\":26.11,\"timezone\":\"")
              + a.zone + QString::fromLatin1("\",\"utc_offset_seconds\":") + QString::number(a.offset);
    if (a.current)
        s += QString::fromLatin1(",\"current\":{\"time\":") + QString::number(a.currentTime)
           + QString::fromLatin1(",\"temperature_2m\":") + num(a.currentTemp)
           + QString::fromLatin1(",\"weather_code\":") + integer(a.currentCode, a.currentCode < 0)
           + QString::fromLatin1(",\"is_day\":") + integer(a.currentLight, a.currentLight < 0)
           + QString::fromLatin1(",\"precipitation\":0}");
    QStringList time, temp, precip, prob, code, day;
    QValueList<Row>::ConstIterator r = a.rows.begin();
    for (int i = 0; r != a.rows.end(); ++r, ++i) {
        time.append(QString::number((*r).time));
        temp.append(num((*r).temp));
        if (a.precipCount < 0 || i < a.precipCount)
            precip.append(num((*r).precip));
        prob.append(num((*r).prob));
        code.append(integer((*r).code, (*r).code < 0));
        day.append(integer((*r).light, (*r).light < 0));
    }
    s += QString::fromLatin1(",\"hourly\":{\"time\":[") + time.join(comma)
       + QString::fromLatin1("],\"temperature_2m\":[") + temp.join(comma)
       + QString::fromLatin1("],\"precipitation\":[") + precip.join(comma)
       + QString::fromLatin1("],\"precipitation_probability\":[") + prob.join(comma)
       + QString::fromLatin1("],\"weather_code\":[") + code.join(comma)
       + QString::fromLatin1("],\"is_day\":[") + day.join(comma) + QString::fromLatin1("]}");
    QStringList dtime, tmax, tmin, dprecip, dprob, dcode, rise, set;
    QValueList<DayRow>::ConstIterator d = a.days.begin();
    for (; d != a.days.end(); ++d) {
        dtime.append(QString::number((*d).time));
        tmax.append(num((*d).tmax));
        tmin.append(num((*d).tmin));
        dprecip.append(num((*d).precip));
        dprob.append(num((*d).prob));
        dcode.append(QString::number((*d).code));
        rise.append(integer((*d).sunrise, !(*d).sunrise));
        set.append(integer((*d).sunset, !(*d).sunset));
    }
    s += QString::fromLatin1(",\"daily\":{\"time\":[") + dtime.join(comma)
       + QString::fromLatin1("],\"weather_code\":[") + dcode.join(comma)
       + QString::fromLatin1("],\"temperature_2m_max\":[") + tmax.join(comma)
       + QString::fromLatin1("],\"temperature_2m_min\":[") + tmin.join(comma)
       + QString::fromLatin1("],\"precipitation_sum\":[") + dprecip.join(comma)
       + QString::fromLatin1("],\"precipitation_probability_max\":[") + dprob.join(comma)
       + QString::fromLatin1("],\"sunrise\":[") + rise.join(comma)
       + QString::fromLatin1("],\"sunset\":[") + set.join(comma)
       + QString::fromLatin1("]}}");
    // as downloaded: without the 0 of a QCString
    QCString text = s.latin1();
    QByteArray bytes;
    bytes.duplicate(text.data(), text.length());
    return bytes;
}

// 2026-11-25 in Bucharest (EET, UTC+2): local midnight
const long midnight = utc(2026, 11, 24, 22, 0);

/** At @p h:@p m local time on 2026-11-25: dry and clear until sunrise
    (7:30), rain 6-12 (0.8 mm an hour), cloudy until sunset (16:40), clear
    after; temperatures 1 to 8; the days rainy (whole day 63, 8/1,
    5.6 mm), for 72 hours from midnight.  Returns the time of now. */
long winter(Answer &a, int h, int m, double currentTemp, int currentCode, int currentLight)
{
    long t = midnight + h * 3600L + m * 60L;
    a.zone = QString::fromLatin1("Europe/Bucharest");
    a.offset = 7200;
    a.current = true;
    a.currentTime = t - t % 900;
    a.currentTemp = currentTemp;
    a.currentCode = currentCode;
    a.currentLight = currentLight;
    a.precipCount = -1;
    a.rows.clear();
    for (int i = 0; i < 72; ++i) {
        int hour = i % 24;
        Row r;
        r.time = midnight + i * 3600L;
        r.temp = hour < 14 ? 1 + hour * 0.5 : 8 - (hour - 14) * 0.5;
        r.precip = 0;
        r.prob = 5;
        r.code = hour <= 6 || hour >= 17 ? 0 : 2;
        r.light = hour >= 8 && hour <= 16 ? 1 : 0;
        if (hour >= 6 && hour <= 12) {
            r.precip = 0.8;
            r.prob = 80;
            r.code = 63;
        }
        a.rows.append(r);
    }
    a.days.clear();
    for (int i = 0; i < 3; ++i) {
        DayRow d;
        d.time = midnight + i * 86400L;
        d.tmax = 8;
        d.tmin = 1;
        d.precip = 5.6;
        d.prob = 80;
        d.code = 63;
        d.sunrise = d.time + 7 * 3600 + 30 * 60;
        d.sunset = d.time + 16 * 3600 + 40 * 60;
        a.days.append(d);
    }
    return t;
}

int failures = 0;

void check(const QString &name, const char *what, const QString &got, const QString &want)
{
    // null or empty: the same
    bool ok = got.isEmpty() ? want.isEmpty() : got == want;
    if (!ok)
        ++failures;
    printf("%s %s %s: \"%s\"%s%s%s\n", ok ? "PASS" : "FAIL", name.latin1(), what,
           got.isEmpty() ? "" : got.latin1(), ok ? "" : ", expected \"",
           ok || want.isEmpty() ? "" : want.latin1(), ok ? "" : "\"");
}

/** Gives @p lib the answer @p a for @p id. */
void give(WeatherLib &lib, const QString &name, const Answer &a, const QString &id = location,
          bool later = false)
{
    if (!lib.setAnswer(id, json(a), later)) {
        ++failures;
        printf("FAIL %s: answer not taken: %s\n", name.latin1(), lib.error(id).latin1());
    }
}

QString field(const QString &line, int i)
{
    QStringList f = QStringList::split(QChar('\t'), line, true);
    return i < int(f.count()) ? f[i] : QString::null;
}

/** The first day of dailyValues(): date, icon, maximum, minimum,
    precipitation, probability, description; and the second as the
    days of the answer have it. */
void checkToday(WeatherLib &lib, const QString &name, const char *date, const char *icon,
                const char *tmax, const char *tmin, const char *precip, const char *prob,
                const char *description, const QString &id = location)
{
    QStringList days = lib.dailyValues(id);
    if (days.count() < 2) {
        ++failures;
        printf("FAIL %s: %d days\n", name.latin1(), int(days.count()));
        return;
    }
    QString today = days[0];
    check(name, "date", field(today, 0), QString::fromLatin1(date));
    check(name, "icon", field(today, 1), QString::fromLatin1(icon));
    check(name, "maximum", field(today, 2), QString::fromLatin1(tmax));
    check(name, "minimum", field(today, 3), QString::fromLatin1(tmin));
    check(name, "precipitation", field(today, 4), QString::fromLatin1(precip));
    check(name, "probability", field(today, 5), QString::fromLatin1(prob));
    check(name, "description", field(today, 6), QString::fromLatin1(description));
    // the other days: the daily values of Open-Meteo, as before
    QString tomorrow = days[1];
    check(name, "tomorrow", field(tomorrow, 1) + '/' + field(tomorrow, 2) + '/'
                            + field(tomorrow, 3) + '/' + field(tomorrow, 4),
          QString::fromLatin1("weather-showers/8.0/1.0/5.60"));
}

/** The current icon and the cards, as the applet and the popup show them. */
void picture(WeatherLib &lib, const QString &name, long t, const QString &dir)
{
    if (dir.isEmpty())
        return;
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
                      .arg(name)
                      .arg(QTime(0, 0).addSecs((t - midnight) % 86400).toString(QString::fromLatin1("hh:mm")))
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
    out.save(dir + '/' + name + QString::fromLatin1(".png"), "PNG");
}

}

int main(int argc, char **argv)
{
    QString png;
    if (argc == 3 && strcmp(argv[1], "--png") == 0)
        png = QString::fromLocal8Bit(argv[2]);

    // never the user's configuration
    char kdehome[] = "/tmp/kweathertodaytest-XXXXXX";
    if (!mkdtemp(kdehome)) {
        perror("mkdtemp");
        return 2;
    }
    setenv("KDEHOME", kdehome, 1);
    setenv("KDE_LANG", "C", 1);
    unsetenv("TZDIR");

    // no KApplication: it would start a DCOP server
    KInstance *instance = new KInstance("kweathertodaytest");
    QApplication *app = 0;
    if (!png.isEmpty()) {
        static int args = 1;
        app = new QApplication(args, argv);
    }

    WeatherLib lib;
    Answer a;
    long t;

    // 20:30, a rainy morning, nothing to come: no rain icon, no amount;
    // the night icon as that of now; 21:00 to 24:00 and now
    t = winter(a, 20, 30, 2.4, 0, 0);
    a.rows[22].code = 1;
    WeatherLib::setClock(t);
    give(lib, "evening", a);
    check("evening", "now", lib.iconName(location), QString::fromLatin1("weather-clear-night"));
    checkToday(lib, "evening", "2026-11-25", "weather-few-clouds-night", "4.5", "1.0", "0.00",
               "5", "Mainly clear");
    picture(lib, "evening", t, png);

    // a rejected answer, an incomplete one: the data from before stays
    {
        QStringList before = lib.dailyValues(location);
        QByteArray bad;
        bad.duplicate("{\"latitude\":", 12);
        bool taken = lib.setAnswer(location, bad);
        check("rejected", "taken", taken ? "yes" : "no", "no");
        check("rejected", "error", lib.error(location).isEmpty() ? "none" : "some", "some");
        check("rejected", "days", lib.dailyValues(location).join("|"), before.join("|"));
        Answer b = a;
        b.rows.clear();
        taken = lib.setAnswer(location, json(b));
        check("incomplete", "taken", taken ? "yes" : "no", "no");
        check("incomplete", "days", lib.dailyValues(location).join("|"), before.join("|"));
        check("incomplete", "now", lib.iconName(location), QString::fromLatin1("weather-clear-night"));
    }

    // 20:30, a null temperature: its rain still counts
    t = winter(a, 20, 30, 2.4, 0, 0);
    a.rows[21].temp = none;
    a.rows[21].precip = 1.0;
    a.rows[21].code = 63;
    give(lib, "null-temperature", a);
    checkToday(lib, "null-temperature", "2026-11-25", "weather-showers", "4.0", "1.0", "1.00",
               "5", "Rain");

    // 14:10, snow codes with 0.02 mm to come (not shown): overcast, not
    // snow; snow showers without an amount: partly cloudy; 0.3 mm: snow
    t = winter(a, 14, 10, -1, 3, 1);
    WeatherLib::setClock(t);
    for (int i = 15; i <= 24; ++i) {
        a.rows[i].temp = -2;
        a.rows[i].code = i < 19 ? 71 : 3;
        a.rows[i].precip = i == 16 ? 0.02 : 0;
    }
    give(lib, "snow-dry", a);
    checkToday(lib, "snow-dry", "2026-11-25", "weather-overcast", "-1.0", "-2.0", "0.02", "5",
               "Overcast");
    a.rows[16].precip = 0;
    give(lib, "snow-dry", a);
    picture(lib, "snow-dry", t, png);
    for (int i = 15; i <= 24; ++i)
        a.rows[i].code = i < 19 ? 85 : 1;
    give(lib, "showers-dry", a);
    checkToday(lib, "showers-dry", "2026-11-25", "weather-clouds", "-1.0", "-2.0", "0.00", "5",
               "Partly cloudy");
    for (int i = 15; i <= 24; ++i)
        a.rows[i].code = i < 19 ? 71 : 3;
    a.rows[16].precip = 0.3;
    give(lib, "snow-wet", a);
    checkToday(lib, "snow-wet", "2026-11-25", "weather-snow", "-1.0", "-2.0", "0.30", "5",
               "Light snow");

    // the threshold: 0.05 mm is shown (snow), 0.04 mm is not (overcast)
    a.rows[16].precip = 0.05;
    give(lib, "threshold-mm", a);
    checkToday(lib, "threshold-mm", "2026-11-25", "weather-snow", "-1.0", "-2.0", "0.05", "5",
               "Light snow");
    a.rows[16].precip = 0.04;
    give(lib, "below-threshold-mm", a);
    checkToday(lib, "below-threshold-mm", "2026-11-25", "weather-overcast", "-1.0", "-2.0",
               "0.04", "5", "Overcast");

    // in inches: 0.002 in = 0.0508 mm is shown, 0.0019 in = 0.048 mm not
    {
        WeatherLib inch;
        inch.setUnits("celsius", "kmh", "inch", "hPa");
        a.rows[16].precip = 0.002;
        give(inch, "threshold-inch", a);
        checkToday(inch, "threshold-inch", "2026-11-25", "weather-snow", "-1.0", "-2.0", "0.00",
                   "5", "Light snow");
        check("threshold-inch", "unit", field(inch.dailyValues(location)[0], 8), "in");
        a.rows[16].precip = 0.0019;
        give(inch, "below-threshold-inch", a);
        checkToday(inch, "below-threshold-inch", "2026-11-25", "weather-overcast", "-1.0", "-2.0",
                   "0.00", "5", "Overcast");
    }

    // misaligned arrays: no amounts after 19:00; unknown is not dry, the
    // snow stays, no amount
    a.rows[16].precip = 0;
    a.precipCount = 20;
    give(lib, "misaligned", a);
    checkToday(lib, "misaligned", "2026-11-25", "weather-snow", "-1.0", "-2.0", "", "5",
               "Light snow");

    // 20:30, the hours end at 22:00: the amount is unknown, the snow
    // stays
    t = winter(a, 20, 30, 2.4, 0, 0);
    WeatherLib::setClock(t);
    while (a.rows.count() > 23)
        a.rows.remove(a.rows.fromLast());
    a.rows[21].code = 71;
    give(lib, "short-data", a);
    checkToday(lib, "short-data", "2026-11-25", "weather-snow", "4.5", "2.4", "", "5",
               "Light snow");

    // 23:20, the last hour: it and the first two of tomorrow (at least
    // 3), not the rain of the hour after them
    t = winter(a, 23, 20, 0.6, 0, 0);
    WeatherLib::setClock(t);
    a.rows[24].temp = 0;
    a.rows[24].code = 0;
    a.rows[25].temp = -1;
    a.rows[25].code = 2;
    a.rows[26].temp = -2;
    a.rows[26].code = 3;
    a.rows[27].temp = -5;
    a.rows[27].code = 61;
    a.rows[27].precip = 1;
    a.rows[27].prob = 90;
    give(lib, "last-hour", a);
    checkToday(lib, "last-hour", "2026-11-25", "weather-overcast", "0.6", "-2.0", "0.00", "5",
               "Overcast");
    picture(lib, "last-hour", t, png);

    // exactly midnight: the new day, its 24 hours (0.1 mm each)
    t = winter(a, 24, 0, 1, 61, 0);
    WeatherLib::setClock(t);
    for (int i = 0; i < 72; ++i) {
        a.rows[i].precip = 0.1;
        a.rows[i].code = 61;
    }
    give(lib, "midnight", a);
    QStringList days = lib.dailyValues(location);
    check("midnight", "date", days.isEmpty() ? QString::null : field(days[0], 0), "2026-11-26");
    check("midnight", "precipitation", days.isEmpty() ? QString::null : field(days[0], 4), "2.40");

    // 05:30, before sunrise: the rain of the morning to come; dry, the
    // clouds of the day, not of the night as now
    t = winter(a, 5, 30, 3.5, 0, 0);
    WeatherLib::setClock(t);
    give(lib, "before-sunrise", a);
    checkToday(lib, "before-sunrise", "2026-11-25", "weather-showers", "8.0", "1.0", "5.60", "80",
               "Rain");
    for (int i = 0; i < 72; ++i) {
        if (a.rows[i].code == 63)
            a.rows[i].code = 2;
        a.rows[i].precip = 0;
        a.rows[i].prob = 5;
    }
    give(lib, "before-sunrise-dry", a);
    checkToday(lib, "before-sunrise-dry", "2026-11-25", "weather-clouds", "8.0", "1.0", "0.00",
               "5", "Partly cloudy");
    picture(lib, "before-sunrise-dry", t, png);
    days = lib.daily(location);
    check("before-sunrise-dry", "text", days.isEmpty() ? QString::null
                                        : field(days[0], 1) + '/' + field(days[0], 4),
          QString::fromLatin1("weather-clouds/"));

    // the sunset at 16:40, not is_day: at 16:30 the sun is up (now and
    // all hours say night), at 16:45 it is down (now says day)
    t = winter(a, 16, 30, 5, 0, 0);
    WeatherLib::setClock(t);
    give(lib, "before-sunset", a);
    checkToday(lib, "before-sunset", "2026-11-25", "weather-clear", "6.5", "1.0", "0.00", "5",
               "Clear sky");
    t = winter(a, 16, 45, 5, 0, 1);
    WeatherLib::setClock(t);
    give(lib, "after-sunset", a);
    checkToday(lib, "after-sunset", "2026-11-25", "weather-clear-night", "6.5", "1.0", "0.00", "5",
               "Clear sky");

    // a short day: sun 11:50-12:10, missed by the hours (is_day 0)
    t = winter(a, 9, 30, 1, 0, 0);
    WeatherLib::setClock(t);
    for (int i = 0; i < 72; ++i) {
        a.rows[i].light = 0;
        a.rows[i].code = 0;
        a.rows[i].precip = 0;
    }
    a.days[0].sunrise = midnight + 11 * 3600 + 50 * 60;
    a.days[0].sunset = midnight + 12 * 3600 + 10 * 60;
    give(lib, "short-day", a);
    checkToday(lib, "short-day", "2026-11-25", "weather-clear", "8.0", "1.0", "0.00", "80",
               "Clear sky");

    // is_day unknown in some hours and no sunrise or sunset: unknown is
    // not day; the night of now and of the known hours
    t = winter(a, 20, 30, 2.4, 0, 0);
    WeatherLib::setClock(t);
    a.rows[21].light = -1;
    a.rows[22].light = -1;
    for (int i = 0; i < 3; ++i)
        a.days[i].sunrise = a.days[i].sunset = 0;
    give(lib, "unknown-day", a);
    checkToday(lib, "unknown-day", "2026-11-25", "weather-clear-night", "4.5", "1.0", "0.00", "5",
               "Clear sky");

    // the hybrid model: ICON-EU ends at 18:00 (wet), ECMWF (1 degree
    // warmer) has the evening: from ECMWF, not the whole day
    {
        WeatherLib h;
        Answer icon, ecmwf;
        t = winter(ecmwf, 20, 30, 2.4, 0, 0);
        WeatherLib::setClock(t);
        for (int i = 0; i < 72; ++i)
            ecmwf.rows[i].temp += 1;
        winter(icon, 20, 30, 2.4, 0, 0);
        while (icon.rows.count() > 19)
            icon.rows.remove(icon.rows.fromLast());
        for (int i = 13; i <= 18; ++i) {
            icon.rows[i].code = 63;
            icon.rows[i].precip = 0.8;
        }
        give(h, "hybrid-expired", icon, hybrid);
        give(h, "hybrid-expired", ecmwf, hybrid, true);
        checkToday(h, "hybrid-expired", "2026-11-25", "weather-clear-night", "5.5", "2.0", "0.00",
                   "5", "Clear sky", hybrid);

        // ICON-EU until 22:00 (and a null temperature at 21:00), ECMWF
        // after it and for that hour: its rain at 23:00 counts
        winter(icon, 20, 30, 2.4, 0, 0);
        while (icon.rows.count() > 23)
            icon.rows.remove(icon.rows.fromLast());
        icon.rows[21].temp = none;
        ecmwf.rows[23].precip = 0.5;
        ecmwf.rows[23].code = 61;
        give(h, "hybrid-partial", icon, hybrid);
        give(h, "hybrid-partial", ecmwf, hybrid, true);
        checkToday(h, "hybrid-partial", "2026-11-25", "weather-showers", "5.5", "2.0", "0.50",
                   "5", "Light rain", hybrid);
    }

    // the end of summer time: 2026-10-25 has 25 hours in Bucharest; at
    // 01:30 EEST 24 of them are still to come (0.1 mm each)
    {
        WeatherLib dst;
        Answer d;
        long start = utc(2026, 10, 24, 21, 0);      // 00:00 EEST
        t = utc(2026, 10, 24, 22, 30);              // 01:30 EEST
        WeatherLib::setClock(t);
        winter(d, 0, 0, 10, 61, 0);
        d.offset = 10800;
        d.currentTime = t;
        for (int i = 0; i < 72; ++i) {
            d.rows[i].time = start + i * 3600L;
            d.rows[i].precip = 0.1;
            d.rows[i].code = 61;
        }
        long day[] = { start, utc(2026, 10, 25, 22, 0), utc(2026, 10, 26, 22, 0) };
        for (int i = 0; i < 3; ++i) {
            d.days[i].time = day[i];
            d.days[i].sunrise = day[i] + 7 * 3600;
            d.days[i].sunset = day[i] + 17 * 3600;
        }
        give(dst, "summer-time-end", d);
        QStringList v = dst.dailyValues(location);
        check("summer-time-end", "date", v.isEmpty() ? QString::null : field(v[0], 0),
              "2026-10-25");
        check("summer-time-end", "precipitation", v.isEmpty() ? QString::null : field(v[0], 4),
              "2.40");
        check("summer-time-end", "days", QString::number(v.count()), "3");
    }

    WeatherLib::setClock(0);
    printf("%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
    delete app;
    delete instance;
    rmdir(kdehome);
    return failures ? 1 : 0;
}
